#include "engine.h"
#include "uinput/uinput.h"

namespace clak {

ClakEngine::ClakEngine(fcitx::Instance* instance)
    : instance_(instance),
      factory_([this](fcitx::InputContext& ic) -> ime::ClakState* {
          return new ime::ClakState(this, &ic);
      }) {
    instance_->inputContextManager().registerProperty("clakState", &factory_);
    // pre-warm uinput device so kernel and libinput enumerate it before first use
    uinput::UinputTool::instance();
}

void ClakEngine::keyEvent(const fcitx::InputMethodEntry& entry, fcitx::KeyEvent& keyEvent) {
    FCITX_UNUSED(entry);
    auto* state = keyEvent.inputContext()->propertyFor(&factory_);
    if (state) {
        state->keyEvent(keyEvent);
    }
}

void ClakEngine::activate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) {
    FCITX_UNUSED(entry);
    auto* state = event.inputContext()->propertyFor(&factory_);
    if (state && event.type() == fcitx::EventType::InputContextFocusOut) {
        if (!state->isBrowser()) {
            state->reset();
        }
    }
}

void ClakEngine::deactivate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) {
    FCITX_UNUSED(entry);
    auto* state = event.inputContext()->propertyFor(&factory_);
    if (state && event.type() == fcitx::EventType::InputContextFocusOut) {
        // preserve composition during browser text-input churn
        if (!state->isBrowser()) {
            state->reset();
        }
    }
}

void ClakEngine::reset(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) {
    FCITX_UNUSED(entry);
    auto* state = event.inputContext()->propertyFor(&factory_);
    if (state && event.type() == fcitx::EventType::InputContextFocusOut) {
        if (!state->isBrowser()) {
            state->reset();
        }
    }
}

std::string ClakEngine::subMode(const fcitx::InputMethodEntry& entry, fcitx::InputContext& ic) {
    FCITX_UNUSED(entry);
    FCITX_UNUSED(ic);
    return "Telex";
}

std::string ClakEngine::subModeIconImpl(const fcitx::InputMethodEntry& entry, fcitx::InputContext& ic) {
    FCITX_UNUSED(entry);
    FCITX_UNUSED(ic);
    return "org.fcitx.Fcitx5.clak";
}

std::string ClakEngine::subModeLabelImpl(const fcitx::InputMethodEntry& entry, fcitx::InputContext& ic) {
    FCITX_UNUSED(entry);
    FCITX_UNUSED(ic);
    return "Vi";
}

} // namespace clak

class ClakEngineFactoryExport : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
        return new clak::ClakEngine(manager->instance());
    }
};

FCITX_ADDON_FACTORY(ClakEngineFactoryExport);
