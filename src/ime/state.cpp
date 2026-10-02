#include "state.h"
#include "engine.h"
#include "config/config.h"
#include "config/sites.h"
#include "platform/window_info.h"
#include "platform/modal_editor.h"
#include "uinput/uinput.h"
#include "utils/log.h"
#include "utils/text_utils.h"

#include <fcitx/inputcontext.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/utf8.h>

namespace clak {
namespace ime {

ClakState::ClakState(ClakEngine* engine, fcitx::InputContext* ic)
    : engine_(engine), ic_(ic) {
    rust_ctx_ = clak_context_new(CLAK_METHOD_TELEX);
}

ClakState::~ClakState() {
    if (rust_ctx_) {
        clak_context_free(rust_ctx_);
        rust_ctx_ = nullptr;
    }
}

void ClakState::reset() {
    std::string app = appKey();
    std::string site = activeSite();
    if (is_deleting_) {
        utils::clakLog("reset: ignored mid-flight uinput deletion (sentinel in flight: " +
                       std::to_string(current_backspace_count_) + "/" + std::to_string(expected_backspaces_) +
                       " pending='" + pending_commit_string_ + "' app=" + app + " site='" + site + "')");
        return;
    }
    last_editor_check_us_ = 0;
    last_text_len_ = 0;
    if (safety_timer_) {
        safety_timer_.reset();
    }
    if (!buffered_keys_.empty() || !pending_commit_string_.empty()) {
        utils::clakLog("reset: clearing state (buf_len=" + std::to_string(buffered_keys_.size()) + ") app=" + app + " site='" + site + "'");
    }
    is_deleting_ = false;
    is_address_bar_fix_ = false;
    expected_backspaces_ = 0;
    current_backspace_count_ = 0;
    pending_commit_string_.clear();
    buffered_keys_.clear();
    verify_.pending = false;
    is_canvas_editor_ = false;
    is_rich_text_editor_ = false;
    mismatch_count_ = 0;
    cached_site_.clear();
    last_site_check_us_ = 0;
    if (rust_ctx_) {
        clak_context_reset(rust_ctx_);
    }
}

std::string ClakState::appKey() {
    if (ic_ && !ic_->program().empty()) {
        return ic_->program();
    }
    platform::WindowInfo win = platform::getActiveWindow();
    if (!win.win_class.empty()) {
        return win.win_class;
    }
    return "default";
}

std::string ClakState::activeSite() {
    uint64_t now_us = fcitx::now(CLOCK_MONOTONIC);
    if (now_us - last_site_check_us_ < config::kCacheCheckIntervalUs && !cached_site_.empty()) {
        return cached_site_;
    }
    last_site_check_us_ = now_us;
    platform::WindowInfo win = platform::getActiveWindow();
    cached_site_ = config::extractDomain(win.win_class, win.win_title);
    return cached_site_;
}

bool ClakState::isBrowser() const {
    std::string app = const_cast<ClakState*>(this)->appKey();
    return config::isBrowserApp(app);
}

bool ClakState::isGecko() const {
    std::string app = const_cast<ClakState*>(this)->appKey();
    return config::isGeckoApp(app);
}

bool ClakState::isCursorNearWord(const fcitx::SurroundingText& surr) {
    if (!surr.isValid()) return false;
    const std::string& text = surr.text();
    if (text.empty()) return false;
    unsigned int cursor = surr.cursor();
    size_t utf8_len = fcitx::utf8::length(text);
    if (cursor >= utf8_len) return false;

    // skip to character at cursor offset
    auto it = text.begin();
    for (unsigned int i = 0; i < cursor && it != text.end(); ++i) {
        uint32_t chr = 0;
        it = fcitx::utf8::getNextChar(it, text.end(), &chr);
    }
    // check remaining characters after cursor
    while (it != text.end()) {
        uint32_t chr = 0;
        it = fcitx::utf8::getNextChar(it, text.end(), &chr);
        if (chr != ' ' && chr != '\t' && chr != '\n' && chr != '\r') {
            return true;
        }
    }
    return false;
}

bool ClakState::shouldUseUinput(bool use_surrounding, uint32_t action_type, const fcitx::SurroundingText& surr) {
    std::string app = appKey();
    std::string site = activeSite();

    if (config::isTerminalApp(app)) {
        return true;
    }
    // gecko apps (zen, firefox) require paced uinput to avoid wayland delete_surrounding_text bugs
    if (config::isGeckoApp(app)) {
        return true;
    }
    // meta sites (facebook, messenger, instagram...) rely strictly on surrounding text
    if (config::isMetaSite(site) || config::isMetaSite(app)) {
        return false;
    }
    if (action_type == CLAK_ACTION_ADDRESS_BAR_FIX) {
        return true;
    }
    if (config::isForceUinputSite(site)) {
        return true;
    }
    if (is_canvas_editor_ || is_rich_text_editor_) {
        return true;
    }
    // switch to uinput when cursor is detected between, before, or after words in browsers
    if (isBrowser() && isCursorNearWord(surr)) {
        return true;
    }
    return !use_surrounding;
}

bool ClakState::isAutofillCertain(const fcitx::SurroundingText& surr) {
    if (!surr.isValid()) return false;
    std::string app = appKey();
    std::string site = activeSite();
    if (config::isMetaSite(site) || config::isMetaSite(app)) {
        return false;
    }
    const std::string& text = surr.text();
    // address bar url autofill does not contain spaces or newlines
    if (text.empty() || text.find('\n') != std::string::npos || text.find(' ') != std::string::npos) return false;

    unsigned int cursor = surr.cursor();
    unsigned int anchor = surr.anchor();

    // selection extends past cursor in single-line context (chromium address bar autocomplete)
    if (cursor != anchor) {
        unsigned int sel_start = std::min(anchor, cursor);
        unsigned int sel_end = std::max(anchor, cursor);
        if (sel_start >= cursor || (sel_start < cursor && sel_end > cursor)) {
            return true;
        }
    }

    return false;
}

void ClakState::setVerifyExpectation(const std::string& wordBefore, size_t delChars, const std::string& added) {
    verify_.pending = true;
    verify_.preWord = wordBefore;
    verify_.del = delChars;
    verify_.added = added;

    std::string base = wordBefore;
    utils::popUtf8Chars(base, delChars);
    verify_.expectWord = base + added;
}

void ClakState::verifySurrounding(const fcitx::SurroundingText& surr) {
    if (!verify_.pending) return;
    verify_.pending = false;

    std::string actualWord = utils::extractWordBeforeCursor(surr.text(), surr.cursor());

    if (actualWord == verify_.expectWord) {
        mismatch_count_ = 0;
    } else {
        mismatch_count_++;
        utils::clakLog("verifySurrounding mismatch #" + std::to_string(mismatch_count_) +
                       ": expected='" + verify_.expectWord + "' actual='" + actualWord + "'");
        if (mismatch_count_ >= 3) {
            is_rich_text_editor_ = true;
            utils::clakLog("verifySurrounding: auto-switched to uinput for this editor");
        }
    }
}

void ClakState::doCommitString(const std::string& text) {
    if (text.empty()) return;
    last_commit_time_us_ = fcitx::now(CLOCK_MONOTONIC);
    ic_->commitString(text);
}

std::string ClakState::classifyGroup(const std::string& app, const std::string& site, bool is_autofill, bool used_uinput) {
    bool has_url_cap = ic_ && ic_->capabilityFlags().test(fcitx::CapabilityFlag::Url);
    if (is_autofill || has_url_cap) return "address-bar";
    if (config::isTerminalApp(app)) return "Terminal-Uinput";
    if (site == "docs.google.com" || site.find("docs.google.com") != std::string::npos) return "Docs-Uinput";
    if (config::isGeckoApp(app)) return "Gecko-Uinput";
    if (used_uinput) return "Chromium-Uinput";
    return "Chromium-SurroundingText";
}

void ClakState::logLatency(const std::string& group, uint64_t start_us, const std::string& action_type) {
    if (start_us == 0) return;
    uint64_t now_us = fcitx::now(CLOCK_MONOTONIC);
    double delta_ms = (now_us >= start_us) ? static_cast<double>(now_us - start_us) / 1000.0 : 0.0;
    utils::clakLog("[LATENCY] group=" + group + " delta_ms=" + std::to_string(delta_ms) + " action=" + action_type);
}

void ClakState::arm_safety_timer() {
    uint64_t now_us = fcitx::now(CLOCK_MONOTONIC);
    uint64_t timeout_us = is_address_bar_fix_ ? (config::kSafetyTimeoutUs * 2) : config::kSafetyTimeoutUs;
    safety_timer_ = engine_->instance()->eventLoop().addTimeEvent(
        CLOCK_MONOTONIC,
        now_us + timeout_us,
        0,
        [this, timeout_us](fcitx::EventSourceTime*, uint64_t) {
            if (is_deleting_) {
                std::string app = appKey();
                std::string site = activeSite();
                utils::clakLog("SENTINEL TIMEOUT (" + std::to_string(timeout_us / 1000) + "ms expired): giving up waiting, only " +
                               std::to_string(current_backspace_count_) + "/" + std::to_string(expected_backspaces_) +
                               " BS returned! force committing '" + pending_commit_string_ + "' app=" + app +
                               " site='" + site + "'");
                is_deleting_ = false;
                is_address_bar_fix_ = false;
                expected_backspaces_ = 0;
                current_backspace_count_ = 0;
                if (!pending_commit_string_.empty()) {
                    doCommitString(pending_commit_string_);
                    pending_commit_string_.clear();
                }
                logLatency(op_group_, op_start_us_, "REPLACE_TIMEOUT");
                op_start_us_ = 0;
                replayBufferedKeys();
            }
            safety_timer_.reset();
            return true;
        }
    );
}

void ClakState::replayBufferedKeys() {
    if (buffered_keys_.empty()) {
        return;
    }
    auto keys = std::move(buffered_keys_);
    buffered_keys_.clear();
    utils::clakLog("replayBufferedKeys: " + std::to_string(keys.size()) + " keys");

    std::string batch_commit;
    auto flush_batch = [this, &batch_commit]() {
        if (!batch_commit.empty()) {
            utils::clakLog("replay batch commit: '" + batch_commit + "'");
            doCommitString(batch_commit);
            batch_commit.clear();
        }
    };

    for (size_t i = 0; i < keys.size(); ++i) {
        const auto& k = keys[i];
        if (is_deleting_) {
            flush_batch();
            buffered_keys_.push_back(k);
            continue;
        }
        if (!handleKey(k)) {
            bool has_ctrl_alt = k.states().test(fcitx::KeyState::Ctrl) ||
                                k.states().test(fcitx::KeyState::Alt) ||
                                k.states().test(fcitx::KeyState::Super);
            std::string str = fcitx::Key::keySymToUTF8(k.sym());
            if (!has_ctrl_alt && !str.empty() && k.sym() < 0xff00) {
                // batch consecutive raw printable characters to minimize wayland round-trips
                batch_commit += str;
            } else {
                flush_batch();
                utils::clakLog("replay forward raw: sym=" + std::to_string(k.sym()));
                ic_->forwardKey(k);
            }
        } else {
            flush_batch();
        }
    }
    flush_batch();
}

void ClakState::updateModalEditorStatus() {
    uint64_t now_us = fcitx::now(CLOCK_MONOTONIC);
    if (now_us - last_editor_check_us_ < config::kCacheCheckIntervalUs) {
        return;
    }
    last_editor_check_us_ = now_us;

    platform::WindowInfo win = platform::getActiveWindow();
    bool was_editor = is_modal_editor_;
    is_modal_editor_ = platform::isEditorActive(win);
    if (!was_editor && is_modal_editor_) {
        editor_mode_ = EditorMode::NORMAL;
        utils::clakLog("modal editor activated: class='" + win.win_class + "' title='" + win.win_title + "' pid=" + std::to_string(win.pid) + " -> mode: NORMAL");
    } else if (was_editor && !is_modal_editor_) {
        utils::clakLog("modal editor deactivated: class='" + win.win_class + "'");
    }
}

bool ClakState::handleKey(const fcitx::Key& key) {
    if (key.isModifier()) {
        return false;
    }

    bool has_ctrl_alt = key.states().test(fcitx::KeyState::Ctrl) ||
                        key.states().test(fcitx::KeyState::Alt) ||
                        key.states().test(fcitx::KeyState::Super);
    bool has_ctrl = key.states().test(fcitx::KeyState::Ctrl);

    std::string key_str = fcitx::Key::keySymToUTF8(key.sym());
    uint32_t sym = key.sym();

    updateModalEditorStatus();

    if (is_modal_editor_) {
        if (editor_mode_ == EditorMode::INSERT) {
            if (sym == FcitxKey_Escape ||
                (has_ctrl && (sym == FcitxKey_bracketleft || sym == FcitxKey_c))) {
                editor_mode_ = EditorMode::NORMAL;
                reset();
                utils::clakLog("editor mode -> NORMAL (via " + key_str + ")");
                return false;
            }
        } else if (editor_mode_ == EditorMode::COMMAND) {
            if (sym == FcitxKey_Return || sym == FcitxKey_KP_Enter ||
                sym == FcitxKey_Escape ||
                (has_ctrl && (sym == FcitxKey_bracketleft || sym == FcitxKey_c))) {
                editor_mode_ = EditorMode::NORMAL;
                reset();
                utils::clakLog("editor mode -> NORMAL (via " + key_str + ")");
                return false;
            }
            reset();
            utils::clakLog("editor COMMAND: forward raw '" + key_str + "'");
            return false;
        } else {
            // normal mode
            if (!has_ctrl_alt) {
                if (sym == FcitxKey_colon || sym == FcitxKey_slash || sym == FcitxKey_question) {
                    editor_mode_ = EditorMode::COMMAND;
                    reset();
                    utils::clakLog("editor mode -> COMMAND (via " + key_str + ")");
                    return false;
                }
                if (sym == FcitxKey_i || sym == FcitxKey_I ||
                    sym == FcitxKey_a || sym == FcitxKey_A ||
                    sym == FcitxKey_o || sym == FcitxKey_O ||
                    sym == FcitxKey_c || sym == FcitxKey_C ||
                    sym == FcitxKey_s || sym == FcitxKey_S ||
                    sym == FcitxKey_R) {
                    editor_mode_ = EditorMode::INSERT;
                    reset();
                    utils::clakLog("editor mode -> INSERT (via " + key_str + ")");
                    return false;
                }
            }
            reset();
            utils::clakLog("editor NORMAL: forward raw '" + key_str + "'");
            return false;
        }
    }

    if (sym == FcitxKey_Escape) {
        reset();
    }

    const char* surr_text = nullptr;
    size_t cursor = 0;
    size_t anchor = 0;

    std::string app = appKey();
    std::string site = activeSite();
    bool is_term = config::isTerminalApp(app);
    bool is_meta = config::isMetaSite(site) || config::isMetaSite(app);
    bool is_force_uinput = config::isForceUinputSite(site);
    bool is_gecko = config::isGeckoApp(app);

    bool has_surrounding = ic_->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText);
    const auto& surr = ic_->surroundingText();

    if (is_meta) {
        is_canvas_editor_ = false;
        is_rich_text_editor_ = false;
    } else if (!is_gecko && has_surrounding && surr.isValid()) {
        const std::string& text = surr.text();
        if (text == "  " || text == "\xc2\xa0\xc2\xa0") {
            is_canvas_editor_ = true;
        } else if (is_canvas_editor_ && text.size() > 2 && text != "  " && text != "\xc2\xa0\xc2\xa0") {
            is_canvas_editor_ = false;
        }

        if (!is_canvas_editor_) {
            if (text == "\n" && surr.cursor() == 0 && surr.anchor() == 0) {
                is_rich_text_editor_ = true;
            }
        }
    }

    // valid surrounding text to pass to rust engine
    bool valid_surr = has_surrounding && surr.isValid() && !is_canvas_editor_ && !is_term;
    if (valid_surr) {
        surr_text = surr.text().c_str();
        cursor = surr.cursor();
        anchor = surr.anchor();
    }
    bool use_surrounding = valid_surr && !is_rich_text_editor_ && !is_force_uinput;

    // gecko surrounding text is async so skip verify to avoid false mismatch
    bool skip_verify = is_gecko || is_meta;
    if (verify_.pending && use_surrounding && !skip_verify) {
        verifySurrounding(surr);
    } else if (verify_.pending) {
        verify_.pending = false;
    }

    if (is_gecko && is_rich_text_editor_) {
        is_rich_text_editor_ = false;
    }

    ClakAction action = clak_process_key(rust_ctx_,
                                         sym,
                                         key_str.c_str(),
                                         has_ctrl_alt,
                                         surr_text,
                                         cursor,
                                         anchor);

    std::string action_name = (action.action_type == CLAK_ACTION_FORWARD ? "FORWARD" :
                              (action.action_type == CLAK_ACTION_COMMIT ? "COMMIT" : "REPLACE"));
    utils::clakLog("handleKey: sym=" + std::to_string(sym) + " ('" + key_str + "') app=" + app +
                   " site='" + site + "' -> " + action_name + " del=" + std::to_string(action.delete_count) +
                   " commit='" + std::string(action.commit_str ? action.commit_str : "") + "'");

    switch (action.action_type) {
        case CLAK_ACTION_FORWARD:
            if (valid_surr) {
                last_text_len_ = surr.cursor() + 1;
            } else {
                last_text_len_ = 1;
            }
            logLatency(classifyGroup(app, site, false, false), op_start_us_, "FORWARD");
            op_start_us_ = 0;
            return false;

        case CLAK_ACTION_COMMIT:
            if (action.commit_str && action.commit_str[0] != '\0') {
                doCommitString(action.commit_str);
            }
            logLatency(classifyGroup(app, site, false, false), op_start_us_, "COMMIT");
            op_start_us_ = 0;
            last_text_len_ = 0;
            return true;

        case CLAK_ACTION_ADDRESS_BAR_FIX:
        case CLAK_ACTION_REPLACE:
        case CLAK_ACTION_REPLACE_SURROUNDING: {
            size_t real_bs = action.delete_count;
            bool is_autofill = (action.action_type == CLAK_ACTION_ADDRESS_BAR_FIX) ||
                               (isBrowser() && isAutofillCertain(surr));
            bool use_uinput = is_autofill || shouldUseUinput(use_surrounding, action.action_type, surr);

            if (use_uinput && real_bs > 0) {
                size_t autofill_extra = is_autofill ? 1 : 0;
                size_t bs_to_send = real_bs + autofill_extra + 1;
                is_address_bar_fix_ = is_autofill;
                op_group_ = classifyGroup(app, site, is_autofill, true);

                uint32_t post_delay = is_autofill ? config::kAddressBarPostDelayMs : (is_term ? config::kTerminalPostDelayMs : 2);
                uint32_t gap_ms = is_term ? config::kTerminalGapMs : 2;
                uint32_t pre_delay = 0;

                utils::clakLog("uinput waiting for sentinel: expected=" + std::to_string(bs_to_send) +
                               " (real=" + std::to_string(real_bs) + (is_autofill ? " + 1 autofill" : "") +
                               " + 1 sentinel) post_delay=" + std::to_string(post_delay) + "ms gap=" +
                               std::to_string(gap_ms) + "ms commit='" +
                               (action.commit_str ? action.commit_str : "") + "' app=" + app + " site='" + site + "'");

                if (uinput::UinputTool::instance().send_backspace(bs_to_send, post_delay, pre_delay, gap_ms)) {
                    is_deleting_ = true;
                    expected_backspaces_ = bs_to_send;
                    current_backspace_count_ = 0;
                    pending_commit_string_ = (action.commit_str ? action.commit_str : "");
                    arm_safety_timer();
                    last_text_len_ = 0;
                    return true;
                } else {
                    utils::clakLog("ERROR: uinput send_backspace failed! app=" + app + " site='" + site + "'");
                }
            }

            // surrounding text path
            if (real_bs > 0) {
                if (use_surrounding) {
                    std::string wordBefore = utils::extractWordBeforeCursor(surr.text(), surr.cursor());
                    setVerifyExpectation(wordBefore, real_bs, action.commit_str ? action.commit_str : "");
                    utils::clakLog("surrounding delete: -" + std::to_string(real_bs) + " commit='" +
                                   (action.commit_str ? action.commit_str : "") + "' app=" + app + " site='" + site + "'");
                    ic_->deleteSurroundingText(-static_cast<int>(real_bs),
                                               static_cast<unsigned int>(real_bs));
                } else {
                    utils::clakLog("forward backspace: count=" + std::to_string(real_bs) +
                                   " commit='" + (action.commit_str ? action.commit_str : "") + "' app=" + app);
                    for (size_t i = 0; i < real_bs; ++i) {
                        ic_->forwardKey(fcitx::Key(FcitxKey_BackSpace));
                    }
                }
            }
            if (action.commit_str && action.commit_str[0] != '\0') {
                doCommitString(action.commit_str);
            }
            logLatency(classifyGroup(app, site, false, false), op_start_us_, "REPLACE");
            op_start_us_ = 0;
            last_text_len_ = 0;
            return true;
        }

        default:
            return false;
    }
}

void ClakState::keyEvent(fcitx::KeyEvent& keyEvent) {
    if (keyEvent.isRelease()) {
        return;
    }

    if (!is_deleting_) {
        op_start_us_ = fcitx::now(CLOCK_MONOTONIC);
    }

    const auto& key = keyEvent.key();
    std::string key_str = fcitx::Key::keySymToUTF8(key.sym());

    // waiting for uinput events to loop back
    if (is_deleting_) {
        if (key.sym() == FcitxKey_BackSpace) {
            current_backspace_count_++;
            if (current_backspace_count_ < expected_backspaces_) {
                return;
            }
            // last sentinel backspace: swallow it, then commit
            keyEvent.filterAndAccept();
            if (safety_timer_) {
                safety_timer_.reset();
            }
            is_deleting_ = false;
            is_address_bar_fix_ = false;
            expected_backspaces_ = 0;
            current_backspace_count_ = 0;

            if (!pending_commit_string_.empty()) {
                doCommitString(pending_commit_string_);
                pending_commit_string_.clear();
            }
            logLatency(op_group_, op_start_us_, "REPLACE");
            op_start_us_ = 0;
            replayBufferedKeys();
            return;
        } else if (key.isModifier()) {
            return;
        } else {
            buffered_keys_.push_back(key);
            keyEvent.filterAndAccept();
            return;
        }
    }

    if (handleKey(key)) {
        keyEvent.filterAndAccept();
    }
}

} // namespace ime
} // namespace clak
