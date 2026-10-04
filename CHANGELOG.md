# Changelog

All notable changes to this project are documented in this file.

## [v0.2.1](https://github.com/versenilvis/clak/releases/tag/v0.2.1) - 2026-10-04

### Bug fixes

- Set _pkgname to ClakIME for archive root folder ([38c957](https://github.com/versenilvis/clak/commit/38c9578fcb1384bf67c89e8e0b90412578b6df65))

## [v0.2.0](https://github.com/versenilvis/clak/releases/tag/v0.2.0) - 2026-10-04

<div align="center">
  <img width="949" height="700" alt="image" src="https://github.com/user-attachments/assets/778c2e42-b4a5-46db-96df-2e1858077601" />
  
  <strong>NEW UI</strong>
  
</div>

### Bug fixes

- Add 50ms guard against false resets from web applications ([cadbb2](https://github.com/versenilvis/clak/commit/cadbb25c8680e8186dd03354ed916fec934f55b0))
- Handle rapid selection deletion and prevent buffering shortcut keys ([3999a3](https://github.com/versenilvis/clak/commit/3999a35d73d754a15e58de6288c35d227ce16980))
- Default excluded apps and macros to empty ([42cf4a](https://github.com/versenilvis/clak/commit/42cf4ac8161d2ebcff000b4f9026712d3e3604b7))
- Guard against stale selection in browser autocomplete detection ([751bdc](https://github.com/versenilvis/clak/commit/751bdc4e9cf138d2d1ca5a2bc570d13fbfc0882b))
- Resolve autostart disable dispatch in clak ([bde478](https://github.com/versenilvis/clak/commit/bde47837436fa705a89d0585341f090c5550019d))
- Correct clak_config_free return type to void in core.h ([bd677c](https://github.com/versenilvis/clak/commit/bd677c2d5fbfa00f90879b185d3f046439100586))
- Clean environment conf when disabling autostart ([3fbe4a](https://github.com/versenilvis/clak/commit/3fbe4a77cd70aa0dab5ed325ce3d4ae38c48b15f))
- Bound per-app state map to prevent unbounded memory growth ([3645af](https://github.com/versenilvis/clak/commit/3645af6306c9582950fce02d18f6b27131a9b100))

### Documentation

- Update installing guide ([8b671c](https://github.com/versenilvis/clak/commit/8b671c2efc3d529844ba7a2d1d4bf1d6a0996f92))
- Add wps proof ([da784b](https://github.com/versenilvis/clak/commit/da784b1558acbbe0ae874761473597d85cfdfaa3))
- Add comprehensive test documentation with test case tables ([0d1870](https://github.com/versenilvis/clak/commit/0d18708e917d25bbcf4c580107e94f728681f351))
- Enrich technical documentation with architecture diagram, invariants, dispatch paths, and API surface ([3fe2b8](https://github.com/versenilvis/clak/commit/3fe2b81fd977e37e852ba4fa15ae7feedb4aa582))
- Adjust capitalization to natural sentence casing across docs ([c42d84](https://github.com/versenilvis/clak/commit/c42d845e07b9f7bd837e13b0c4e38a3f79951894))
- Add api.md reference manual covering C-FFI surface and data structures ([782322](https://github.com/versenilvis/clak/commit/7823224ec690e326b8a494661ac68f5922724598))
- Replace absolute file URI links with relative repository paths ([695e93](https://github.com/versenilvis/clak/commit/695e93636f21a3acc2b7fd9a0ed8879166be5aff))

### Features

- Add configuration system, inotify reload, and shortcuts ([7f5830](https://github.com/versenilvis/clak/commit/7f58301f0b1c73967798ce8e449d9ee784dba9ca))
- Add clak bench CLI with group filtering and p99 regression assertion ([740950](https://github.com/versenilvis/clak/commit/74095031540ad10db5dbb1535b4739f8675c0c72))
- Reset composition on mouse click via libinput tracker ([6e8660](https://github.com/versenilvis/clak/commit/6e8660b79fa5875c311b61efa812b02ed45b33d9))
- Add clak doctor environment diagnostic utility and just recipe ([c92886](https://github.com/versenilvis/clak/commit/c9288698e428601daea3f3055f5f86f5365a4963))
- Add standalone uninstall script ([bc12ed](https://github.com/versenilvis/clak/commit/bc12ed2900b4034516d64f4b146a75367e95b5ee))
- Add wps office compatibility detection, wrappers, and benchmark tooling ([9ec566](https://github.com/versenilvis/clak/commit/9ec566467d612230218c79eac6b81d361d99c016))
- Add autostart management and update configuration ([857321](https://github.com/versenilvis/clak/commit/8573215be42a9e775821b81908bc5ac14e70ee54))
- Add autostart management subcommand and doctor check ([bbdc3c](https://github.com/versenilvis/clak/commit/bbdc3c80c66e1ea865a3ab89340c8f0b4f13ba25))
- Configure autostart on install and cleanup on uninstall ([4f9a39](https://github.com/versenilvis/clak/commit/4f9a3945abe124e116596badf3bc1d3501520822))
- Support macro expansion with case matching and enter tab triggers ([071485](https://github.com/versenilvis/clak/commit/071485ed8fab3a82f215fa076d39c474f8cbe339))
- Separate safety timeout into fast 50ms default and 250ms selection deletion ([016bc1](https://github.com/versenilvis/clak/commit/016bc15055b8fcd3037eba5cf9d79930f559be66))
- Implement auto capitalize after sentence boundary ([c6beaa](https://github.com/versenilvis/clak/commit/c6beaa98dd57ff43c28ace50dcc3c2dfa9a9031c))
- Add adaptive wait latency tracking for dynamic safety recovery ([0cf313](https://github.com/versenilvis/clak/commit/0cf31306af6a697415799f1f5b96e9a4ed4292de))
- Support fallback to fcitx program and detect terminal editor processes on gnome ([74252d](https://github.com/versenilvis/clak/commit/74252d5ef610d08ca56f43f72434ac652ea9e8c5))
- Add clak-gui Slint configuration application ([175073](https://github.com/versenilvis/clak/commit/175073131401d541e8229e29104153ddc333676c))
- Add settings action to tray icon menu ([abdbca](https://github.com/versenilvis/clak/commit/abdbcaa423f05a20f16f46b5bb1601d26c160c32))

### Performance

- Narrow mutex lock to emission and release during thread sleeps ([364888](https://github.com/versenilvis/clak/commit/364888955b3953bdee113426ae05bf0a432bbb87))

### Refactors

- Rename bin to cli ([3d1ae8](https://github.com/versenilvis/clak/commit/3d1ae8b07321f11ae10d41dcb2e119f184279952))

### Security

- Auto vuln scan ([226ade](https://github.com/versenilvis/clak/commit/226ade324032876558738d52d1567491f5397813))
- Verify sha256 checksums before archive extraction ([95c19c](https://github.com/versenilvis/clak/commit/95c19c156b8e9718110b4bbf8bd4da45b2fb3e06))

## [v0.1.0](https://github.com/versenilvis/clak/releases/tag/v0.1.0) - 2026-10-02

### Bug fixes

- Detect wayland and gtk4 terminals ([d648d1](https://github.com/versenilvis/clak/commit/d648d188d17f8fec4a567e27f3148e30d761a7a5))
- Wrong cursor detector ([41e410](https://github.com/versenilvis/clak/commit/41e4101734f87b3c91a7c0a488c9c194c00c7448))
- Accidentally triggered wrong flag ([298504](https://github.com/versenilvis/clak/commit/2985048d09bc8b7f02b0f2cd1a89f4d9bebe0ae7))
- Fix typing over selection and clippy ([b63766](https://github.com/versenilvis/clak/commit/b6376625f76c1f9d0c6ec752fcc55e8d02dbdbae))
- Set cargoRoot for engine ([cea1a3](https://github.com/versenilvis/clak/commit/cea1a3f1fa3a139d5edd71239f83f8ec396f970e))

### Documentation

- Telegram and neovim ([979166](https://github.com/versenilvis/clak/commit/9791668314056bb9bc18531ebc76eb4d2ee32d57))
- Removed test message ([02f68e](https://github.com/versenilvis/clak/commit/02f68e04b1c4ad295b1b77f70d8f2d58a5277bf6))
- Fix neovim format ([ae3159](https://github.com/versenilvis/clak/commit/ae3159f134c4b896171e560e5f9b4511b0b6245e))
- Document cursor near word and why surrounding over uinput ([4a4d0d](https://github.com/versenilvis/clak/commit/4a4d0d6008869a92cf6e670b01fac41d986eaeb8))
- Add vscode family pacing and xterm.js explanation ([92fe5c](https://github.com/versenilvis/clak/commit/92fe5c5d1744012485585c82f0ff6877f63439fc))

### Features

- Clak input method ([456376](https://github.com/versenilvis/clak/commit/4563768f17a774cde88597532dd2f678d4e97c90))
- Cursor detector ([982ba6](https://github.com/versenilvis/clak/commit/982ba676cd07b75888e9df223f1ef2135a67f3ad))
- Detect vscode family ([7ed102](https://github.com/versenilvis/clak/commit/7ed102f204e65de8ff28cd92ac8d58f5c60c008e))
- Install and update ([4650f3](https://github.com/versenilvis/clak/commit/4650f3e3ba0ff01049679c3c5d0c72ee9e3fd0db))
