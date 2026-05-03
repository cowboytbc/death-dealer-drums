# DEATH DEALER DRUMS Installers

This folder contains installer templates and helper scripts modeled after the existing INFERNO TONES plugin installers.

## Windows (Inno Setup)

Files:
- `windows/DEATH_DEALER_DRUMS_Installer.iss`
- `windows/Build-DEATH-DEALER-DRUMS-Installer.ps1`

Expected build inputs:
- `DELIVERABLES/Windows/VST3/DEATH DEALER DRUMS.vst3`
- `DELIVERABLES/Windows/Standalone/DEATH DEALER DRUMS.exe`

Build the installer:
1. Install Inno Setup 6 (so `iscc` is available).
2. Run `windows/Build-DEATH-DEALER-DRUMS-Installer.ps1`.

## macOS (Installer helper package)

Files:
- `macos/Install-DEATH-DEALER-DRUMS-Mac.command`
- `macos/Prepare-DEATH-DEALER-DRUMS-Mac-Installer.sh`
- `macos/Build-DEATH-DEALER-DRUMS-macOS-pkg.sh`
- `macos/distribution.xml`
- `macos/Generate-LOGO-icns.sh`

Expected build inputs:
- `DELIVERABLES/macOS/VST3/DEATH DEALER DRUMS.vst3`
- `DELIVERABLES/macOS/AU/DEATH DEALER DRUMS.component` (optional)
- `DELIVERABLES/macOS/Standalone/DEATH DEALER DRUMS.app`

Prepare a distributable folder on macOS:
- Run `macos/Prepare-DEATH-DEALER-DRUMS-Mac-Installer.sh`

Build the signed/distributable-style macOS installer package (`.pkg`) like the other INFERNO TONES plugins:
- (Optional but recommended) Run `macos/Generate-LOGO-icns.sh` before mac build to create `LOGO.icns` from `LOGO.png`
- Run `macos/Build-DEATH-DEALER-DRUMS-macOS-pkg.sh 1.0.0`
- Output: `release/DEATH-DEALER-DRUMS-macOS-1.0.0.pkg`

The `.pkg` includes three selectable components:
- VST3 → `/Library/Audio/Plug-Ins/VST3`
- Audio Unit → `/Library/Audio/Plug-Ins/Components`
- Standalone app → `/Applications`

## Standalone icon

`CMakeLists.txt` now sets:
- `ICON_BIG` to `LOGO.png`
- `ICON_SMALL` to `LOGO.png`

On macOS, if `LOGO.icns` exists in project root, it will be embedded into `DEATH DEALER DRUMS.app` as the bundle icon.

## GitHub Actions release automation

This project now includes `.github/workflows/release.yml`.

It builds and publishes:
- Windows installer `.exe` (Inno Setup)
- macOS installer `.pkg` (pkgbuild + productbuild)

Trigger options:
- Push a tag like `v1.0.0`
- Or run manually via **workflow_dispatch** and pass `tag`

Optional notarized macOS releases:
- Set workflow input `notarize_macos=true`
- Add repository secrets:
	- `APPLE_DEV_ID_APP_CERT_P12_BASE64`
	- `APPLE_DEV_ID_APP_CERT_PASSWORD`
	- `APPLE_ID`
	- `APPLE_APP_SPECIFIC_PASSWORD`
	- `APPLE_TEAM_ID`
