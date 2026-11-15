package org.theoraclesproject;

import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Insets;
import android.graphics.Rect;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.OpenableColumns;
import android.app.Activity;
import android.view.DisplayCutout;
import android.view.HapticFeedbackConstants;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.RoundedCorner;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;

import org.libsdl.app.SDLActivity;

/* The launcher's activity: SDL's, which loads libSDL3.so then libmain.so and runs main on its own thread. */
public class OraclesActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }

    /* The system bars kept hidden in fullscreen.  SDL hides them when its window goes fullscreen (immersive sticky) and
     * again 2 s after Android says they show, but Android does not always say so (the window given the focus back after
     * the notification shade or a system dialog), and SDL's call does nothing when the view keeps the flags it sets: the
     * status bar could stay over the game.  The bars are hidden again when the window has the focus back, on resume, and
     * when they show while it has the focus, after a delay in which bars a swipe showed hide by themselves. */
    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().getDecorView().setOnApplyWindowInsetsListener((view, insets) -> {
            if (!barsShown(view, insets).isEmpty()) hideBarsLater("shown with the focus", 3000);
            return view.onApplyWindowInsets(insets);
        });
        getSystemService(InputManager.class).registerInputDeviceListener(devices, null);
    }

    @Override
    protected void onDestroy() {
        getWindow().getDecorView().removeCallbacks(hideBars);
        getSystemService(InputManager.class).unregisterInputDeviceListener(devices);
        super.onDestroy();
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideBarsLater("resumed", 500);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideBarsLater("focus regained", 0);
    }

    private String barsReason = "";
    private final Runnable hideBars = () -> hideSystemBars(barsReason);

    private void hideBarsLater(String reason, int delayMs) {
        final View decor = getWindow().getDecorView();
        barsReason = reason;
        decor.removeCallbacks(hideBars);
        decor.postDelayed(hideBars, delayMs);
    }

    private long barsLoggedAt = -60000;
    private int barsUnlogged;

    private void hideSystemBars(String reason) {
        final Window window = getWindow();
        final View decor = window.getDecorView();
        final WindowInsets insets = decor.getRootWindowInsets();
        final String shown = insets != null ? barsShown(decor, insets) : "";
        if (!mFullscreenModeActive || !hasWindowFocus() || shown.isEmpty()) return;
        if (Build.VERSION.SDK_INT >= 30) {
            final WindowInsetsController controller = window.getInsetsController();
            if (controller == null) return;
            controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            controller.hide(WindowInsets.Type.systemBars());
        } else {
            decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.INVISIBLE);
        }
        /* A line every ten seconds at most, saying how many it stands for. */
        final long now = SystemClock.uptimeMillis();
        if (now - barsLoggedAt < 10000) { barsUnlogged++; return; }
        log("oracles: system bars hidden again (" + shown + " shown, " + reason + ")"
            + (barsUnlogged > 0 ? ", " + barsUnlogged + " more time(s) since the last line" : ""));
        barsLoggedAt = now;
        barsUnlogged = 0;
    }

    /* The bars shown ("status bar", "navigation bar", both), or "" when none is; before Android 11, by the flags the
     * system clears when it shows them. */
    private static String barsShown(View decor, WindowInsets insets) {
        final boolean status, navigation;
        if (Build.VERSION.SDK_INT >= 30) {
            status = insets.isVisible(WindowInsets.Type.statusBars());
            navigation = insets.isVisible(WindowInsets.Type.navigationBars());
        } else {
            final int flags = decor.getSystemUiVisibility();
            status = (flags & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0;
            navigation = (flags & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) == 0;
        }
        return status && navigation ? "status bar and navigation bar" : status ? "status bar" : navigation ? "navigation bar" : "";
    }

    /* A line in the launcher's log, written by libmain.so (launcher/main.c), which may have failed to load. */
    private static native void nativeLog(String line);

    private static void log(String line) {
        try {
            nativeLog(line);
        } catch (UnsatisfiedLinkError ignored) {
        }
    }

    /* Back is the launcher's Back, as the system's is.  SDL makes a controller's Back a controller's "back" button, the
     * one its Select gives too, the game's Select: a handheld whose Back belongs to its built-in controller would have no
     * Back, nor would a keyboard that has arrows (the emulator's, many a hardware keyboard), which Android gives the DPAD
     * source and SDL takes for a controller.  A controller that has no Select of its own, whose select button Android
     * reports as Back, keeps it as the game's Select, SDL's.  Select stays SDL's, a game button.  A mouse's Back (a side
     * button) stays SDL's too, which ignores it; and every key when SDL's libraries could not be loaded, which SDL's
     * activity leaves to Android, nothing native being there to call. */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (mBrokenLibraries) return super.dispatchKeyEvent(event);
        final InputDevice device = event.getDevice();
        final boolean controller = controller(device);
        boolean hasSelect = false;
        if (controller) hasSelect = hasSelect(device);
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            if (controller) { controllerKeys++; lastKeyDevice = device.getName(); }
            else otherKeys++;
        }
        final boolean mouse = (event.getSource() & InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE;
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK && !mouse && (!controller || hasSelect)) {
            if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
                backSource = (device != null ? device.getName() : "no device") + (controller(device) ? ", a controller" : "")
                    + ", source 0x" + Integer.toHexString(event.getSource());
                SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_BACK);
            } else if (event.getAction() == KeyEvent.ACTION_UP) SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_BACK);
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    /* The device the last Back came from (its name, whether it is a controller, the event's source), for the line the
     * launcher writes when a Back opens the pause menu (launcher/sdl_backend.c). */
    private static volatile String backSource = "";

    public static String backSource() {
        return backSource;
    }

    /* Whether a controller has a Select of its own, by its device id, asked of Android once and said in the log, again
     * after the device changes. */
    private final HashMap<Integer, Boolean> selects = new HashMap<>();

    private boolean hasSelect(InputDevice device) {
        final Boolean known = selects.get(device.getId());
        if (known != null) return known;
        final boolean has = device.hasKeys(KeyEvent.KEYCODE_BUTTON_SELECT)[0];
        selects.put(device.getId(), has);
        log("oracles: controller " + device.getName() + ": " + (has ? "has Select, its Back opens the menu" : "no Select, its Back is the game's Select"));
        return has;
    }

    private final InputManager.InputDeviceListener devices = new InputManager.InputDeviceListener() {
        @Override public void onInputDeviceAdded(int id) { selects.remove(id); }
        @Override public void onInputDeviceRemoved(int id) { selects.remove(id); }
        @Override public void onInputDeviceChanged(int id) { selects.remove(id); }
    };

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

    /* What the system takes of the window, for the launcher's log beside SDL's safe area (launcher/safe_area.c): the
     * window's bounds and the largest it could have, the insets of each kind (left, top, right, bottom) and those of the
     * bars were they shown, whether the bars show, the cutout's rectangles, the rounded corners' radii, the cutout mode
     * and the system UI flags SDL's fullscreen set.  Called on SDL's thread, as are cutoutInsets and displayDensity: they
     * read the window's state and change nothing. */
    public static String safeArea() {
        final Activity activity = (Activity) getContext();
        if (activity == null) return "";
        final Window window = activity.getWindow();
        final View decor = window != null ? window.getDecorView() : null;
        if (decor == null) return "";
        final StringBuilder s = new StringBuilder("android: view ").append(decor.getWidth()).append('x').append(decor.getHeight());
        if (Build.VERSION.SDK_INT >= 30) {
            s.append(", window ").append(rect(activity.getWindowManager().getCurrentWindowMetrics().getBounds()))
             .append(" of ").append(rect(activity.getWindowManager().getMaximumWindowMetrics().getBounds()));
        }
        s.append(", display ").append(activity.getWindowManager().getDefaultDisplay().getDisplayId());
        final WindowInsets insets = decor.getRootWindowInsets();
        if (insets != null && Build.VERSION.SDK_INT >= 30) {
            s.append(", cutout ").append(insets(insets.getInsets(WindowInsets.Type.displayCutout())))
             .append(", status bar ").append(insets(insets.getInsets(WindowInsets.Type.statusBars())))
             .append(insets.isVisible(WindowInsets.Type.statusBars()) ? " shown" : " hidden")
             .append(", navigation bar ").append(insets(insets.getInsets(WindowInsets.Type.navigationBars())))
             .append(insets.isVisible(WindowInsets.Type.navigationBars()) ? " shown" : " hidden")
             .append(", bars when shown ").append(insets(insets.getInsetsIgnoringVisibility(WindowInsets.Type.systemBars())))
             .append(", gestures ").append(insets(insets.getInsets(WindowInsets.Type.systemGestures())))
             .append(", mandatory gestures ").append(insets(insets.getInsets(WindowInsets.Type.mandatorySystemGestures())))
             .append(", tappable ").append(insets(insets.getInsets(WindowInsets.Type.tappableElement())));
        } else if (insets != null) {
            s.append(", system window ").append(insets.getSystemWindowInsetLeft()).append(',').append(insets.getSystemWindowInsetTop())
             .append(',').append(insets.getSystemWindowInsetRight()).append(',').append(insets.getSystemWindowInsetBottom());
        }
        final DisplayCutout cutout = insets != null ? insets.getDisplayCutout() : null;
        if (cutout != null) {
            s.append(", cutout rectangles");
            for (Rect r : cutout.getBoundingRects()) s.append(' ').append(rect(r));
            if (Build.VERSION.SDK_INT >= 30) s.append(", waterfall ").append(insets(cutout.getWaterfallInsets()));
        }
        if (insets != null && Build.VERSION.SDK_INT >= 31) {
            s.append(", corners");
            for (int position = RoundedCorner.POSITION_TOP_LEFT; position <= RoundedCorner.POSITION_BOTTOM_LEFT; position++) {
                final RoundedCorner corner = insets.getRoundedCorner(position);
                s.append(' ').append(corner != null ? corner.getRadius() : 0);
            }
        }
        s.append(", cutout mode ").append(window.getAttributes().layoutInDisplayCutoutMode)
         .append(", UI flags 0x").append(Integer.toHexString(decor.getSystemUiVisibility()));
        return s.toString();
    }

    /* A short tick for a touch control pressed (launcher/touch_sdl.c), asked on a thread of its own and given on the
     * activity's: the view's touch feedback of an on-screen key, which Android gives as its system's touch feedback
     * setting says, no permission needed, nothing where the device has no vibrator.  Ticks asked faster than the
     * activity's thread gives them make one. */
    private static final Runnable giveTouchTick = () -> {
        final Activity activity = (Activity) getContext();
        final Window window = activity != null ? activity.getWindow() : null;
        if (window != null) window.getDecorView().performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
    };

    public static void touchTick() {
        final Activity activity = (Activity) getContext();
        final Window window = activity != null ? activity.getWindow() : null;
        if (window == null) return;
        final View decor = window.getDecorView();
        decor.removeCallbacks(giveTouchTick);
        decor.post(giveTouchTick);
    }

    /* The display cutout's safe insets at the window's top and bottom, "top bottom" in pixels, a curved edge's included:
     * the band the launcher keeps the game's frame, its controls and its pages in (launcher/safe_area.c). */
    public static String cutoutInsets() {
        final Activity activity = (Activity) getContext();
        final Window window = activity != null ? activity.getWindow() : null;
        final WindowInsets insets = window != null ? window.getDecorView().getRootWindowInsets() : null;
        final DisplayCutout cutout = insets != null ? insets.getDisplayCutout() : null;
        return cutout != null ? cutout.getSafeInsetTop() + " " + cutout.getSafeInsetBottom() : "0 0";
    }

    /* The display's pixels per dp (its DisplayMetrics' density): the size of the screen margin's small and large
     * margins (launcher/safe_area.c). */
    public static String displayDensity() {
        final Activity activity = (Activity) getContext();
        return activity != null ? Float.toString(activity.getResources().getDisplayMetrics().density) : "1";
    }

    private static String rect(Rect r) {
        return r.left + "," + r.top + " " + r.width() + "x" + r.height();
    }

    private static String insets(Insets i) {
        return i.left + "," + i.top + "," + i.right + "," + i.bottom;
    }

    private static boolean controller(InputDevice device) {
        if (device == null) return false;
        final int sources = device.getSources();
        return (sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
            || (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
    }

    /* The update check's request (launcher/update_fetch.c), on its thread: a GET of an https:// address, the system's
     * TLS checking the certificate, with an Accept header and a User-Agent without the version, a redirection followed
     * to https:// only.  "STATUS\nBODY", the body cut at a megabyte, or "0\nREASON". */
    public static String httpGet(String address, int timeoutMs) {
        HttpURLConnection connection = null;
        try {
            final URL url = new URL(address);
            if (!"https".equals(url.getProtocol())) return "0\nnot an https:// address";
            connection = (HttpURLConnection) url.openConnection();
            connection.setConnectTimeout(timeoutMs);
            connection.setReadTimeout(timeoutMs);
            connection.setUseCaches(false);
            connection.setRequestProperty("Accept", "application/vnd.github+json");
            connection.setRequestProperty("User-Agent", "the-oracles-project");
            final int status = connection.getResponseCode();
            final InputStream in = status >= 400 ? connection.getErrorStream() : connection.getInputStream();
            final ByteArrayOutputStream body = new ByteArrayOutputStream();
            if (in != null) {
                try (InputStream stream = in) {
                    final byte[] chunk = new byte[16384];
                    for (int read; (read = stream.read(chunk)) > 0 && body.size() <= 1024 * 1024;) body.write(chunk, 0, read);
                }
            }
            return status + "\n" + new String(body.toByteArray(), StandardCharsets.UTF_8);
        } catch (Exception e) {
            return "0\n" + e.getClass().getSimpleName();
        } finally {
            if (connection != null) connection.disconnect();
        }
    }

    /* Export save…: the document picker asked to make a document named `name` where the player chooses, its answer
     * given to the launcher (nativeDocumentCreated), which writes the copy there (launcher/file_dialog.c).  False when
     * no picker opens.  The request's number is none of SDL's own dialogs', which count from 0. */
    private static final int CREATE_DOCUMENT = 0x4f52;

    private static native void nativeDocumentCreated(String uri);

    public static boolean createDocument(String name) {
        final Activity activity = (Activity) getContext();
        if (activity == null) return false;
        final Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/octet-stream");
        intent.putExtra(Intent.EXTRA_TITLE, name);
        try {
            activity.startActivityForResult(intent, CREATE_DOCUMENT);
        } catch (ActivityNotFoundException e) {
            log("oracles: no document picker to export the save: " + e.getMessage());
            return false;
        }
        return true;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != CREATE_DOCUMENT) {
            super.onActivityResult(requestCode, resultCode, data);
            return;
        }
        final Uri uri = resultCode == RESULT_OK && data != null ? data.getData() : null;
        try {
            nativeDocumentCreated(uri != null ? uri.toString() : null);
        } catch (UnsatisfiedLinkError ignored) {
        }
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
