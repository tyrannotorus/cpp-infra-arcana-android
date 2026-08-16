# Infra Arcana (Android)

## Description

Infra Arcana (Android) is a native Android port of [Infra Arcana](https://gitlab.com/martin-tornqvist/ia),
Martin Törnqvist's Lovecraftian horror roguelike. It focuses primarily on a mobile-friendly experience.

<img width="2340" height="1080" alt="Screenshot_20260810_113047_Infra Arcana" src="https://github.com/user-attachments/assets/e629c835-7850-4cb6-a17b-2a5e01281c34" />
<img width="2340" height="1080" alt="Screenshot_20260810_113125_Infra Arcana" src="https://github.com/user-attachments/assets/1a88b2da-9a9b-49fe-b0eb-489daa969378" />
<img width="2340" height="1080" alt="Screenshot_20260810_113206_Infra Arcana" src="https://github.com/user-attachments/assets/238633dd-0542-4879-8480-cafe1abd7e2d" />
<img width="2340" height="1080" alt="Screenshot_20260810_113237_Infra Arcana" src="https://github.com/user-attachments/assets/a17c5523-e183-4058-8f3f-be33050230c0" />
<img width="2340" height="1080" alt="Screenshot_20260810_113356_Infra Arcana" src="https://github.com/user-attachments/assets/2d2a6824-4de2-4d59-acdd-6e12aadcd866" />

## User Disclaimer

Infra Arcana (Android) is shared free with the roguelike community and provided **as-is, without any
warranty**. It is an independent personal-use app; use it at your own risk. The author accepts no
liability when Cthulhu devours your soul.

## Dev Disclaimer

While I am a 10+ year industry veteran, note this project has been human-directed as far as top-down
architecture, but 100% slop-coded at the ground floor. I've done my best to ensure professional code
standards, but I do not guarantee it like I would my own work. Thus is the warning, so beware when
forking. Here may be dragons.

## APK Releases

- Get them here!
  https://github.com/tyrannotorus/cpp-infra-arcana-android/releases

## Supported Platforms

- Android devices
- I have no other devices to test with. Send me one! :)

## Supported Firmware

- Android 7.0 (Nougat, API 24) and newer. Built against Android 15 (API 35).

## Controls

| Gesture | Action |
|---|---|
| Swipe (8 directions) | Move |
| Tap | Enter (confirm/select) |
| Hold briefly, then drag | Look: pans the map with the highlighted center tile described live (a quick flick is a move instead) |
| Back button | Escape |
| Hamburger button | In-game menu: Actions, Video, Audio, Input, Gameplay, Tome of Wisdom, Quit |

## Build

    cd android && ./gradlew assembleStaging
    # -> android/app/build/outputs/apk/staging/app-staging.apk

Toolchain pins: Gradle 8.14.3 · AGP 8.7.3 · compileSdk/targetSdk 35 · minSdk 24 ·
NDK 28.0.13004108 · CMake 3.22.1. The Android SDK/NDK components install on demand
via `sdkmanager`; everything else (SDL2, SDL2_image, SDL2_mixer, game data) is
vendored in-tree, so there are no other dependencies to fetch.

Staging builds are debug-signed. Release builds (`./gradlew assembleRelease`) are
signed only if an `android/keystore.properties` (gitignored) provides a keystore:

    storeFile=/path/to/release.keystore
    storePassword=...
    keyAlias=release
    keyPassword=...

## Releasing

The tagged source must already carry the version: F-Droid builds the tag as-is, so
CI must not rewrite it. Commit these together, then tag:

- `android/app/build.gradle.kts` — `versionCode` (MAJOR × 1000000 + MINOR × 10000 +
  PATCH × 100 + BUILD) and `versionName` (the tag without its `v`)
- `src/version.cpp` — the build increment in `g_build_version_str`, keeping any phase
  suffix (`.4-alpha`); `g_version_str` stays at the vendored upstream version
- `fastlane/metadata/android/en-US/changelogs/<versionCode>.txt`

Tag the commit `vMAJOR.MINOR.PATCH.BUILD` (e.g. `v23.0.0.13` — no phase suffix in the
tag), then draft and publish a GitHub release for it. CI verifies the committed version
against the tag and fails the release if any of the above is missing or stale, then
builds the release APK, signs it with the release key held in repository secrets
(`KEYSTORE_BASE64`, `KEYSTORE_PASSWORD`, `KEY_PASSWORD`; key alias `release`), and
attaches it to the release.

## How the port works

- **No new dependencies:** SDL2 2.30.9, SDL2_image 2.8.2 and SDL2_mixer 2.8.0 are built
  as shared libraries from the sources IA already vendors in `third_party/SDL/src/`
  (PNG via stb_image, OGG via stb_vorbis). SDL's Java glue (`SDLActivity`) is sourced
  directly from the vendored `SDL2-2.30.9/android-project/` tree.
- **Android project:** `android/` (Gradle Kotlin DSL); native build via
  `android/app/src/main/jni/CMakeLists.txt`, which compiles all of `src/*.cpp` into
  `libmain.so` (`SDL_main` is provided by force-including `SDL_main.h` on `main.cpp`).
- **Assets:** the whole `installed_files/` tree is packed into the APK; at first run
  `jni/android_bootstrap.cpp` chdirs to internal storage and extracts it (driven by a
  build-time sha256 manifest, re-extracts on content change), so the game's
  cwd-relative data paths work unmodified. Saves go to SDL's pref path as on desktop.

## License

Infra Arcana is by [Martin Törnqvist](https://gitlab.com/martin-tornqvist/ia) and is
licensed under the **GNU Affero General Public License v3.0 or later**, as is this port.
This is an unofficial port, not affiliated with or endorsed by upstream — report issues
with this port here, never to upstream.

### Copyright

This port is a fork, so the two copyrights sit side by side under the one license:

- **Copyright Martin Törnqvist** &lt;m.tornq@gmail.com&gt; — Infra Arcana itself, and every
  file in this repository derived from it.
- **Copyright Werewolf Camp** — the Android port's own source files, i.e. those with no
  upstream counterpart (the touch interface, display separation, and the rest of the
  mobile work).

Each source file's header names its holder, and carries
`SPDX-License-Identifier: AGPL-3.0-or-later` either way. New port-original files take the
Werewolf Camp line; files inherited from upstream keep Martin's, whether or not this port
has modified them.

### Credits

- **Infra Arcana** by Martin Törnqvist — the game itself: code, art, audio and design.
  [Homepage](https://sites.google.com/site/infraarcana) ·
  [Source](https://gitlab.com/martin-tornqvist/ia). AGPL-3.0-or-later.
- **Android port** by Werewolf Camp — touch interface and mobile build. AGPL-3.0-or-later.
- **SDL2 / SDL2_image / SDL2_mixer** — Sam Lantinga and contributors (zlib license), vendored.
- Per-file asset and library licenses ship with the game data in `installed_files/`.
