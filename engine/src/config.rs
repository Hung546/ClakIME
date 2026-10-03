use serde::{Deserialize, Serialize};
use std::fs;
use std::path::PathBuf;

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ClakConfig {
    #[serde(default)]
    pub general: GeneralConfig,
    #[serde(default)]
    pub spelling: SpellingConfig,
    #[serde(default)]
    pub typing: TypingConfig,
    #[serde(default)]
    pub shortcuts: ShortcutConfig,
    #[serde(default)]
    pub per_app: PerAppConfig,
    #[serde(default)]
    pub macros: MacroConfig,
    #[serde(default)]
    pub advanced: AdvancedConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GeneralConfig {
    #[serde(default = "default_method")]
    pub method: String,
    #[serde(default = "default_charset")]
    pub charset: String,
    #[serde(default = "default_true")]
    pub short_w: bool,
    #[serde(default = "default_true")]
    pub bracket_brackets: bool,
    #[serde(default = "default_startup_mode")]
    pub startup_mode: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SpellingConfig {
    #[serde(default)]
    pub enabled: bool,
    #[serde(default)]
    pub auto_restore: bool,
    #[serde(default = "default_true")]
    pub modern_tone: bool,
    #[serde(default = "default_true")]
    pub free_marking: bool,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct TypingConfig {
    #[serde(default)]
    pub double_space_period: bool,
    #[serde(default)]
    pub auto_capitalize: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShortcutConfig {
    #[serde(default = "default_toggle_shortcut")]
    pub toggle_vietnamese: String,
    #[serde(default = "default_switch_shortcut")]
    pub switch_method: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PerAppConfig {
    #[serde(default = "default_true")]
    pub remember_state: bool,
    #[serde(default = "default_excluded_apps")]
    pub excluded_apps: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MacroConfig {
    #[serde(default = "default_true")]
    pub enabled: bool,
    #[serde(default = "default_macro_items")]
    pub items: Vec<MacroItem>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MacroItem {
    pub trigger: String,
    pub replace: String,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct AdvancedConfig {
    #[serde(default)]
    pub uinput_ack: bool,
    #[serde(default)]
    pub debug_log: bool,
}

fn default_method() -> String {
    "telex".to_string()
}

fn default_charset() -> String {
    "unicode".to_string()
}

fn default_true() -> bool {
    true
}

fn default_startup_mode() -> String {
    "vietnamese".to_string()
}

fn default_toggle_shortcut() -> String {
    "ctrl_shift".to_string()
}

fn default_switch_shortcut() -> String {
    "ctrl_space".to_string()
}

fn default_excluded_apps() -> Vec<String> {
    vec![
        "kitty".to_string(),
        "alacritty".to_string(),
        "foot".to_string(),
        "wezterm".to_string(),
    ]
}

fn default_macro_items() -> Vec<MacroItem> {
    vec![MacroItem {
        trigger: "vn".to_string(),
        replace: "Việt Nam".to_string(),
    }]
}

impl Default for GeneralConfig {
    fn default() -> Self {
        Self {
            method: default_method(),
            charset: default_charset(),
            short_w: default_true(),
            bracket_brackets: default_true(),
            startup_mode: default_startup_mode(),
        }
    }
}

impl Default for SpellingConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            auto_restore: false,
            modern_tone: default_true(),
            free_marking: default_true(),
        }
    }
}

impl Default for ShortcutConfig {
    fn default() -> Self {
        Self {
            toggle_vietnamese: default_toggle_shortcut(),
            switch_method: default_switch_shortcut(),
        }
    }
}

impl Default for PerAppConfig {
    fn default() -> Self {
        Self {
            remember_state: default_true(),
            excluded_apps: default_excluded_apps(),
        }
    }
}

impl Default for MacroConfig {
    fn default() -> Self {
        Self {
            enabled: default_true(),
            items: default_macro_items(),
        }
    }
}

pub fn config_path() -> PathBuf {
    if let Ok(dir) = std::env::var("XDG_CONFIG_HOME") {
        if !dir.is_empty() {
            return PathBuf::from(dir).join("clak").join("config.toml");
        }
    }
    if let Ok(home) = std::env::var("HOME") {
        return PathBuf::from(home)
            .join(".config")
            .join("clak")
            .join("config.toml");
    }
    PathBuf::from("/tmp/clak_config.toml")
}

impl ClakConfig {
    pub fn load() -> Self {
        let path = config_path();
        if path.exists() {
            if let Ok(content) = fs::read_to_string(&path) {
                if let Ok(cfg) = toml::from_str::<ClakConfig>(&content) {
                    return cfg;
                }
            }
        }
        let default_cfg = ClakConfig::default();
        let _ = default_cfg.save();
        default_cfg
    }

    pub fn save(&self) -> Result<(), std::io::Error> {
        let path = config_path();
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        let content = toml::to_string_pretty(self).map_err(std::io::Error::other)?;
        fs::write(path, content)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_default_config_serialize_deserialize() {
        let cfg = ClakConfig::default();
        let toml_str = toml::to_string_pretty(&cfg).expect("serialize");
        let parsed: ClakConfig = toml::from_str(&toml_str).expect("deserialize");
        assert_eq!(parsed.general.method, "telex");
        assert_eq!(parsed.shortcuts.toggle_vietnamese, "ctrl_shift");
        assert_eq!(parsed.per_app.excluded_apps.len(), 4);
    }

    #[test]
    fn test_custom_config_parse() {
        let raw = r#"
[general]
method = "vni"
short_w = false

[per_app]
remember_state = false
excluded_apps = ["myterminal", "code"]
"#;
        let parsed: ClakConfig = toml::from_str(raw).expect("deserialize");
        assert_eq!(parsed.general.method, "vni");
        assert!(!parsed.general.short_w);
        assert!(!parsed.per_app.remember_state);
        assert_eq!(parsed.per_app.excluded_apps, vec!["myterminal", "code"]);
        assert_eq!(parsed.shortcuts.toggle_vietnamese, "ctrl_shift");
    }
}
