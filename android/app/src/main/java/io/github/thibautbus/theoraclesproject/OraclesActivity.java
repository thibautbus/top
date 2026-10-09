package io.github.thibautbus.theoraclesproject;

import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.app.Activity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import org.libsdl.app.SDLActivity;

/* The launcher's activity: SDL's, which loads libSDL3.so then libmain.so and runs main on its own thread. */
public class OraclesActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }

    /* Back from a keyboard that has arrows (the emulator's, many a hardware keyboard: Android gives them the DPAD
     * source) is the launcher's Back, as the system's is: SDL would take any DPAD device for a controller and make its
     * Back a controller's "back" button, the game's Select.  A real controller's Back stays SDL's, a game button. */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            if (controller(event.getDevice())) { controllerKeys++; lastKeyDevice = event.getDevice().getName(); }
            else otherKeys++;
        }
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK && !controller(event.getDevice())) {
            if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_BACK);
            else if (event.getAction() == KeyEvent.ACTION_UP) SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_BACK);
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if ((event.getSource() & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) controllerMotions++;
        return super.dispatchGenericMotionEvent(event);
    }

    /* What reached the activity since the last call, for the launcher's report after a game: the keys pressed on a
     * controller and elsewhere, the controllers' motions, the view that has the focus and the window's focus. */
    private static volatile int controllerKeys, otherKeys, controllerMotions;
    private static volatile String lastKeyDevice = "";

    public static String inputState() {
        final Activity activity = (Activity) getContext();
        final View focus = activity != null ? activity.getCurrentFocus() : null;
        final String state = "controller keys " + controllerKeys + " (" + lastKeyDevice + "), other keys " + otherKeys
            + ", controller motions " + controllerMotions + ", focused view " + (focus != null ? focus.getClass().getSimpleName() : "none")
            + ", window focus " + (activity != null && activity.hasWindowFocus()) + ", display "
            + (activity != null ? activity.getWindowManager().getDefaultDisplay().getDisplayId() : -1);
        controllerKeys = otherKeys = controllerMotions = 0;
        return state;
    }

    private static boolean controller(InputDevice device) {
        if (device == null) return false;
        final int sources = device.getSources();
        return (sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
            || (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
    }

    /* The name the document picker shows for a content:// URI ("Oracle of Ages.gbc"), or null: the launcher copies a
     * chosen ROM or patch under that name (launcher/file_dialog.c). */
    public static String displayName(String uri) {
        try (Cursor cursor = getContext().getContentResolver().query(Uri.parse(uri),
                new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) return cursor.getString(0);
        } catch (Exception ignored) {
        }
        return null;
    }
}
