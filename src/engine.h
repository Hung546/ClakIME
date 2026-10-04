#ifndef CLAK_ENGINE_H
#define CLAK_ENGINE_H

#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include "ime/state.h"

namespace clak {
namespace platform {
class MouseTracker;
}

class ClakEngine : public fcitx::InputMethodEngineV2 {
public:
    ClakEngine(fcitx::Instance* instance);
    ~ClakEngine() override;

    void keyEvent(const fcitx::InputMethodEntry& entry, fcitx::KeyEvent& keyEvent) override;
    void activate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override;
    void deactivate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override;
    void reset(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override;

    std::string subMode(const fcitx::InputMethodEntry& entry, fcitx::InputContext& ic) override;
    std::string subModeIconImpl(const fcitx::InputMethodEntry& entry, fcitx::InputContext& ic) override;
    std::string subModeLabelImpl(const fcitx::InputMethodEntry& entry, fcitx::InputContext& ic) override;

    fcitx::Instance* instance() { return instance_; }

    void loadConfig();
    void setupConfigWatcher();
    const ClakConfig* config() const { return config_; }
    uint64_t configVersion() const { return config_version_; }
    void setConfigForTest(ClakConfig* config) {
        if (config_) {
            clak_config_free(config_);
        }
        config_ = config;
        config_version_++;
    }

    bool isAppEnabled(const std::string& app);
    void toggleAppEnabled(const std::string& app);
    void setAppEnabled(const std::string& app, bool enabled);
    void onMouseClick();

private:
    fcitx::Instance* instance_;
    fcitx::FactoryFor<ime::ClakState> factory_;

    ClakConfig* config_{nullptr};
    std::unique_ptr<fcitx::EventSourceIO> config_io_;
    int inotify_fd_{-1};
    int inotify_wd_{-1};
    static constexpr size_t kMaxTrackedAppStates = 256;
    std::unordered_map<std::string, bool> app_states_;
    bool global_enabled_{true};
    uint64_t config_version_{0};
    std::unique_ptr<platform::MouseTracker> mouse_tracker_;
};

class ClakEngineFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
        return new ClakEngine(manager->instance());
    }
};

} // namespace clak

#endif // CLAK_ENGINE_H
