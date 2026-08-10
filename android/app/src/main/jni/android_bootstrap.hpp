// =============================================================================
// Android port: first-run setup.
//
// IA reads all game data (gfx, audio, data, manual.txt) via paths relative to
// the current working directory. On Android those files live inside the APK,
// where plain file APIs can't reach them. This bootstrap chdirs to the app's
// internal storage and extracts the bundled data there once (re-extracting
// when the APK's asset manifest changes), after which every relative path in
// the game works unmodified.
// =============================================================================

#ifndef ANDROID_BOOTSTRAP_HPP
#define ANDROID_BOOTSTRAP_HPP

namespace android_bootstrap {

// Must run before any game init (paths, config, io).
void init();

}  // namespace android_bootstrap

#endif  // ANDROID_BOOTSTRAP_HPP
