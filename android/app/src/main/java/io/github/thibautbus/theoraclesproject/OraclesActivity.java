package io.github.thibautbus.theoraclesproject;

import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.view.InputDevice;
import android.view.KeyEvent;

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
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK && !controller(event.getDevice())) {
            if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_BACK);
            else if (event.getAction() == KeyEvent.ACTION_UP) SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_BACK);
            return true;
        }
        return super.dispatchKeyEvent(event);
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
