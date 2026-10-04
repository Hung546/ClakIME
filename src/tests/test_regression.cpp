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
    // when loopback sentinel is lost, safety timer must recover state
    MockInputContext ic(instance_->inputContextManager(), "zen");
    ime::ClakState state(engine_.get(), &ic);

    uinput::UinputTool::instance().setMockHandler([](size_t, uint32_t, uint32_t, uint32_t) {
        return true;
    });

    ic.setSurrounding("", 0, 0);
    ic.typeChar('d', &state);
    ic.typeChar('d', &state);

    EXPECT_TRUE(state.isDeleting());

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

} // namespace test
} // namespace clak
