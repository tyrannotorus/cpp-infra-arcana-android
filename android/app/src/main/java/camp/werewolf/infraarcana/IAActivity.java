package camp.werewolf.infraarcana;

import android.os.Build;
import android.os.Bundle;
import android.view.HapticFeedbackConstants;
import android.view.View;
import android.view.WindowInsets;

import org.libsdl.app.SDLActivity;

public class IAActivity extends SDLActivity {
    // Height in screen pixels of the area covered by the on-screen
    // keyboard, or 0 when it is not shown. Written on the UI thread by the
    // window inset listener below, read from the native thread (see
    // io::screen_keyboard_covered_px_h) - hence volatile.
    private static volatile int sScreenKeyboardPxH = 0;

    // Called from native code
    public static int screenKeyboardHeightPx() {
        return sScreenKeyboardPxH;
    }

    // Called from native code (see io::haptic_feedback) on the game thread -
    // posted to the view, since haptics are dispatched through the view
    // hierarchy. Honours the system's touch feedback setting, so it is
    // silent for players who have that off.
    public static void performHapticTick(final boolean isLongPress) {
        final View view = mSurface;

        if (view == null) {
            return;
        }

        view.post(() -> view.performHapticFeedback(
                isLongPress
                        ? HapticFeedbackConstants.LONG_PRESS
                        : HapticFeedbackConstants.CLOCK_TICK));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // The window never resizes for the keyboard (the SDL surface is
        // fullscreen), so the covered height has to be observed from the
        // IME inset. It is dispatched to fullscreen windows as well, and
        // follows the keyboard's open and close animation. The insets are
        // only observed here - they are passed on untouched.
        getWindow().getDecorView().setOnApplyWindowInsetsListener(
                (view, insets) -> {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                        sScreenKeyboardPxH =
                                insets.getInsets(WindowInsets.Type.ime())
                                        .bottom;
                    }

                    return view.onApplyWindowInsets(insets);
                });
    }

    @Override
    protected String[] getLibraries() {
        // Order matters: SDLActivity dlopens the last entry as the main
        // library and calls its SDL_main.
        return new String[] {
            "SDL2",
            "SDL2_image",
            "SDL2_mixer",
            "main",
        };
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        // SDLActivity only finish()es the Activity when SDL_main returns.
        // The process - with the native library's static state already torn
        // down - would survive and be REUSED on the next launch, coming up
        // broken. Kill it so every launch starts clean. (SDLActivity's
        // onDestroy has already joined the native thread here.) The task
        // card is dropped by autoRemoveFromRecents, see the manifest -
        // finish() alone leaves it in the switcher, which reads as the game
        // having merely been backgrounded.
        android.os.Process.killProcess(android.os.Process.myPid());
    }
}
