package camp.werewolf.infraarcana;

import android.os.Build;
import android.os.Bundle;
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
        // The process - with the native library's static state already
        // torn down - would survive and be REUSED on the next launch,
        // coming up broken (and "Exit" in the title menu looked like mere
        // backgrounding). Kill the process so every launch starts clean.
        // (SDLActivity.onDestroy has already joined the native thread at
        // this point.)
        android.os.Process.killProcess(android.os.Process.myPid());
    }
}
