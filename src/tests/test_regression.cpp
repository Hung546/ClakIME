#include <gtest/gtest.h>
#include <fcitx/instance.h>
#include <fcitx-utils/event.h>
#include "mock_input_context.h"
#include "ime/state.h"
#include "engine.h"
#include "core.h"
#include "uinput/uinput.h"
#include "platform/window_info.h"

namespace clak {
namespace test {

class RegressionCorpusTest : public ::testing::Test {
protected:
    void SetUp() override {
        int argc = 1;
        char arg0[] = "clak_regression_test";
        char* argv[] = {arg0, nullptr};
        instance_ = std::make_unique<fcitx::Instance>(argc, argv);
        engine_ = std::make_unique<ClakEngine>(instance_.get());
        engine_->setConfigForTest(clak_config_default());
        uinput::UinputTool::instance().setMockHandler([](size_t, uint32_t, uint32_t, uint32_t) {
            return true;
        });
        platform::setMockActiveWindow(platform::WindowInfo{"google-chrome", "Standard Page", 1234});
    }

    void TearDown() override {
        uinput::UinputTool::instance().clearMockHandler();
        platform::clearMockActiveWindow();
        engine_.reset();
        instance_.reset();
    }

    std::unique_ptr<fcitx::Instance> instance_;
    std::unique_ptr<ClakEngine> engine_;
};

TEST_F(RegressionCorpusTest, test_regression_address_bar_dd_to_d) {
    // address bar autocomplete highlight requires extra backspace and uinput routing
    MockInputContext ic(instance_->inputContextManager(), "google-chrome");
    ic.setUrlCapability(true);
    ime::ClakState state(engine_.get(), &ic);

    size_t uinput_bs_sent = 0;
    uinput::UinputTool::instance().setMockHandler([&](size_t count, uint32_t, uint32_t, uint32_t) {
        uinput_bs_sent = count;
        return true;
    });

    ic.setSurrounding("", 0, 0);
    ic.typeChar('d', &state);

    // simulate address bar autocomplete: single line text with selection extending past cursor
    ic.setSurrounding("ddos.com", 1, 8);
    EXPECT_TRUE(state.isAutofillCertain(ic.surroundingText()));

    ic.typeChar('d', &state);

    // expect 3 backspaces: 1 real + 1 autofill clear + 1 sentinel
    EXPECT_EQ(uinput_bs_sent, 3);
    EXPECT_TRUE(state.isDeleting());

    ic.sendCleanBackspace(&state);
    ic.sendCleanBackspace(&state);
    ic.sendCleanBackspace(&state);

    EXPECT_FALSE(state.isDeleting());
    ASSERT_FALSE(ic.commits.empty());
    EXPECT_EQ(ic.commits.back(), "đ");
}

TEST_F(RegressionCorpusTest, test_regression_hyprland_stale_selection_during_retoning) {
    // hyprland/niri temporary mid-word selection must not be detected as address bar autocomplete
    MockInputContext ic(instance_->inputContextManager(), "google-chrome");
    ic.setUrlCapability(true);
    ime::ClakState state(engine_.get(), &ic);

    // cursor and anchor inside "dựng" (length 6) should not trigger autofill
    ic.setSurrounding("dựng", 3, 4);
    EXPECT_FALSE(state.isAutofillCertain(ic.surroundingText()));
    ic.setSurrounding("dựng", 4, 3);
    EXPECT_FALSE(state.isAutofillCertain(ic.surroundingText()));

    // selection not extending to line end is not autocomplete
    ic.setSurrounding("google more", 2, 6);
    EXPECT_FALSE(state.isAutofillCertain(ic.surroundingText()));

    // reverse direction autocomplete extending to end is valid
    ic.setSurrounding("google", 6, 2);
    EXPECT_TRUE(state.isAutofillCertain(ic.surroundingText()));
}

TEST_F(RegressionCorpusTest, test_regression_docs_cascading_chars) {
    // google docs must route to uinput to avoid cascading duplicate characters
    MockInputContext ic(instance_->inputContextManager(), "google-chrome");
    ime::ClakState state(engine_.get(), &ic);

    platform::setMockActiveWindow(platform::WindowInfo{
        "google-chrome",
        "Google Docs - Document",
        1234
    });

    ic.setSurrounding("nguoi", 5, 5);
    EXPECT_TRUE(state.shouldUseUinput(true, CLAK_ACTION_REPLACE, ic.surroundingText()));
}

TEST_F(RegressionCorpusTest, test_regression_laf_sentinel_timeout) {
    // when loopback sentinel is lost during standard typing, safety timer recovers fast (~50ms)
    platform::setMockActiveWindow(platform::WindowInfo{"zen", "Zen Browser", 1234});
    MockInputContext ic(instance_->inputContextManager(), "zen");
    ime::ClakState state(engine_.get(), &ic);

    uinput::UinputTool::instance().setMockHandler([](size_t, uint32_t, uint32_t, uint32_t) {
        return true;
    });

    ic.setSurrounding("", 0, 0);
    ic.typeChar('d', &state);
    ic.typeChar('d', &state);

    EXPECT_TRUE(state.isDeleting());
    EXPECT_FALSE(state.isSelectionDeletion());

    auto exit_timer = instance_->eventLoop().addTimeEvent(
        CLOCK_MONOTONIC,
        fcitx::now(CLOCK_MONOTONIC) + 300000,
        0,
        [this](fcitx::EventSourceTime*, uint64_t) {
            instance_->eventLoop().exit();
            return true;
        }
    );
    instance_->eventLoop().exec();

    EXPECT_FALSE(state.isDeleting());
    ASSERT_FALSE(ic.commits.empty());
    EXPECT_EQ(ic.commits.back(), "đ");
}

TEST_F(RegressionCorpusTest, test_regression_rapid_selection_deletion) {
    // rapid selection deletion uses the 250ms timeout scenario to prevent race conditions
    platform::setMockActiveWindow(platform::WindowInfo{"zen", "Zen Browser", 1234});
    MockInputContext ic(instance_->inputContextManager(), "zen");
    ime::ClakState state(engine_.get(), &ic);

    uinput::UinputTool::instance().setMockHandler([](size_t, uint32_t, uint32_t, uint32_t) {
        return true;
    });

    // simulate text selection shortcut (shift + left arrow)
    fcitx::Key shift_left(FcitxKey_Left, fcitx::KeyState::Shift);
    fcitx::KeyEvent key_event(&ic, shift_left, false);
    state.keyEvent(key_event);

    ic.setSurrounding("text", 0, 4);
    ic.typeChar('d', &state);
    ic.typeChar('d', &state);

    EXPECT_TRUE(state.isDeleting());
    EXPECT_TRUE(state.isSelectionDeletion());

    // at 100ms, the 250ms selection timer has not fired yet (unlike standard 50ms timer)
    bool still_deleting_at_100ms = false;
    auto mid_timer = instance_->eventLoop().addTimeEvent(
        CLOCK_MONOTONIC,
        fcitx::now(CLOCK_MONOTONIC) + 100000,
        0,
        [&](fcitx::EventSourceTime*, uint64_t) {
            still_deleting_at_100ms = state.isDeleting();
            return true;
        }
    );

    // at 600ms, the 250ms selection timer has fired and recovered state cleanly
    auto exit_timer = instance_->eventLoop().addTimeEvent(
        CLOCK_MONOTONIC,
        fcitx::now(CLOCK_MONOTONIC) + 600000,
        0,
        [this](fcitx::EventSourceTime*, uint64_t) {
            instance_->eventLoop().exit();
            return true;
        }
    );
    instance_->eventLoop().exec();

    EXPECT_TRUE(still_deleting_at_100ms);
    EXPECT_FALSE(state.isDeleting());
    ASSERT_FALSE(ic.commits.empty());
    EXPECT_EQ(ic.commits.back(), "đ");
}

TEST_F(RegressionCorpusTest, test_regression_safety_timer_self_reset_crash) {
    // safety timer callback must not call reset() on itself, avoiding sd-event use-after-free crash
    platform::setMockActiveWindow(platform::WindowInfo{"zen", "Zen Browser", 1234});
    MockInputContext ic(instance_->inputContextManager(), "zen");
    ime::ClakState state(engine_.get(), &ic);

    uinput::UinputTool::instance().setMockHandler([](size_t, uint32_t, uint32_t, uint32_t) {
        return true;
    });

    ic.setSurrounding("", 0, 0);
    ic.typeChar('d', &state);
    ic.typeChar('d', &state);

    EXPECT_TRUE(state.isDeleting());

    // run event loop past the 50ms safety timer timeout without throwing EventLoopException
    EXPECT_NO_THROW({
        auto exit_timer = instance_->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC,
            fcitx::now(CLOCK_MONOTONIC) + 300000,
            0,
            [this](fcitx::EventSourceTime*, uint64_t) {
                instance_->eventLoop().exit();
                return true;
            }
        );
        instance_->eventLoop().exec();
    });

    EXPECT_FALSE(state.isDeleting());
    ASSERT_FALSE(ic.commits.empty());
    EXPECT_EQ(ic.commits.back(), "đ");
}

TEST_F(RegressionCorpusTest, test_regression_gecko_stale_surrounding) {
    // gecko apps must always use uinput and never call deleteSurroundingText
    MockInputContext ic(instance_->inputContextManager(), "firefox");
    ime::ClakState state(engine_.get(), &ic);

    EXPECT_TRUE(state.isGecko());
    ic.setSurrounding("o", 1, 1);
    EXPECT_TRUE(state.shouldUseUinput(true, CLAK_ACTION_REPLACE, ic.surroundingText()));
}

TEST_F(RegressionCorpusTest, test_regression_twitter_draftjs_dd) {
    // draftjs rich text editors switch to uinput after 3 surrounding text mismatches
    MockInputContext ic(instance_->inputContextManager(), "google-chrome");
    ime::ClakState state(engine_.get(), &ic);

    size_t uinput_calls = 0;
    uinput::UinputTool::instance().setMockHandler([&](size_t, uint32_t, uint32_t, uint32_t) {
        uinput_calls++;
        return true;
    });

    // simulate 3 consecutive stale reports from draftjs
    for (int i = 0; i < 3; ++i) {
        state.reset();
        ic.clearHistory();
        ic.setSurrounding("", 0, 0);
        ic.typeChar('d', &state);
        ic.typeChar('d', &state);
        ic.setSurrounding("stale", 5, 5);
        ic.typeChar(' ', &state);
    }

    EXPECT_TRUE(state.isRichTextEditor());

    // 4th replacement must use uinput
    state.reset();
    ic.clearHistory();
    ic.setSurrounding("", 0, 0);
    ic.typeChar('d', &state);
    ic.typeChar('d', &state);

    EXPECT_GT(uinput_calls, 0);
    EXPECT_TRUE(ic.deletions.empty());
}

TEST_F(RegressionCorpusTest, test_regression_terminal_forward_key_dup) {
    // terminal apps must always use uinput to avoid key event duplication
    MockInputContext ic(instance_->inputContextManager(), "kitty");
    ime::ClakState state(engine_.get(), &ic);

    ic.setSurrounding("d", 1, 1);
    EXPECT_TRUE(state.shouldUseUinput(true, CLAK_ACTION_REPLACE, ic.surroundingText()));
}

TEST_F(RegressionCorpusTest, test_regression_adaptive_wait_scales_and_decays) {
    // adaptive latency wait scales up under heavy lag and decays back down when stable
    MockInputContext ic(instance_->inputContextManager(), "zen");
    ime::ClakState state(engine_.get(), &ic);

    EXPECT_EQ(state.adaptiveExtraWaitUs(), 0);

    // fast roundtrip (15ms) maintains 0 extra wait
    state.observeTransactionLatency(15000);
    EXPECT_EQ(state.adaptiveExtraWaitUs(), 0);

    // laggy roundtrip (40ms) scales up by 10ms
    state.observeTransactionLatency(40000);
    EXPECT_EQ(state.adaptiveExtraWaitUs(), 10000);

    // repeated lag scales up further
    state.observeTransactionLatency(45000);
    EXPECT_EQ(state.adaptiveExtraWaitUs(), 20000);

    // 4 consecutive stable transactions trigger a decay step (-10ms)
    for (int i = 0; i < 4; ++i) {
        state.observeTransactionLatency(12000);
    }
    EXPECT_EQ(state.adaptiveExtraWaitUs(), 10000);

    // 4 more stable transactions decay back to 0
    for (int i = 0; i < 4; ++i) {
        state.observeTransactionLatency(12000);
    }
    EXPECT_EQ(state.adaptiveExtraWaitUs(), 0);
}

TEST_F(RegressionCorpusTest, test_regression_newline_does_not_trigger_rich_text_editor) {
    // an empty line with "\n" and cursor 0 must not switch to rich text / uinput mode
    MockInputContext ic(instance_->inputContextManager(), "google-chrome");
    ime::ClakState state(engine_.get(), &ic);

    ic.setSurrounding("\n", 0, 0);
    ic.typeChar('t', &state);
    EXPECT_FALSE(state.isRichTextEditor());

    // continue typing 'h', 'e', 'e', 's' ("thees" -> "thế")
    ic.setSurrounding("t\n", 1, 1);
    ic.typeChar('h', &state);
    ic.setSurrounding("th\n", 2, 2);
    ic.typeChar('e', &state);

    // 2nd 'e' turns "the" into "thê"
    ic.setSurrounding("the\n", 3, 3);
    ic.typeChar('e', &state);
    EXPECT_FALSE(state.isRichTextEditor());
    EXPECT_FALSE(ic.deletions.empty());
    EXPECT_EQ(ic.deletions.back().first, -1);
    EXPECT_EQ(ic.deletions.back().second, 1);
    ASSERT_FALSE(ic.commits.empty());
    EXPECT_EQ(ic.commits.back(), "ê");

    // 's' turns "thê" into "thế"
    ic.setSurrounding("thê\n", 3, 3);
    ic.typeChar('s', &state);
    EXPECT_FALSE(state.isRichTextEditor());
    EXPECT_EQ(ic.deletions.back().first, -1);
    EXPECT_EQ(ic.deletions.back().second, 1);
    EXPECT_EQ(ic.commits.back(), "ế");
}

} // namespace test
} // namespace clak


