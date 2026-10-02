#ifndef CLAK_ENGINE_H
#define CLAK_ENGINE_H

#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include "ime/state.h"

namespace clak {

class ClakEngine : public fcitx::InputMethodEngineV2 {
public:
    ClakEngine(fcitx::Instance* instance);
    ~ClakEngine() override = default;

    void keyEvent(const fcitx::InputMethodEntry& entry, fcitx::KeyEvent& keyEvent) override;
    void activate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override;
    void deactivate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override;
    void reset(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override;

    std::string subMode(const fcitx::InputMethodEntry& entry, fcitx::InputContext& ic) override;
    std::string subModeIconImpl(const fcitx::InputMethodEntry& entry, fcitx::InputContext& ic) override;
    std::string subModeLabelImpl(const fcitx::InputMethodEntry& entry, fcitx::InputContext& ic) override;

    fcitx::Instance* instance() { return instance_; }

private:
    fcitx::Instance* instance_;
    fcitx::FactoryFor<ime::ClakState> factory_;
};

class ClakEngineFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
        return new ClakEngine(manager->instance());
    }
};

} // namespace clak

#endif // CLAK_ENGINE_H
