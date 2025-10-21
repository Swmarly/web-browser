// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static android.view.Display.INVALID_DISPLAY;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Build;
import android.util.Pair;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.WindowInsetsUtils;

/** Handles launching new popup windows as CCTs. */
@NullMarked
public class PopupCreator {
    public static final String EXTRA_REQUESTED_WINDOW_FEATURES =
            "chrome.browser.app.tab_activity_glue.PopupCreator.EXTRA_REQUESTED_WINDOW_FEATURES";

    private static final String TAG = "PopupCreator";
    private static @Nullable Boolean sArePopupsEnabledForTesting;
    private static @Nullable ReparentingTask sReparentingTaskForTesting;
    private static @Nullable Insets sInsetsForecastForTesting;

    public static void moveTabToNewPopup(Tab tab, WindowFeatures windowFeatures) {
        Intent intent = initializePopupIntent();
        ActivityOptions activityOptions =
                createPopupActivityOptions(windowFeatures, tab.getWindowAndroid());
        intent.putExtra(EXTRA_REQUESTED_WINDOW_FEATURES, windowFeatures.toBundle());

        getReparentingTask(tab)
                .begin(
                        ContextUtils.getApplicationContext(),
                        intent,
                        activityOptions.toBundle(),
                        null);
    }

    // TODO(https://crbug.com/411002260): retrieve the display from bounds when Android Display
    // Topology API is available to Chrome
    public static boolean arePopupsEnabled(DisplayAndroid display) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)) {
            return false;
        }

        if (sArePopupsEnabledForTesting != null) {
            return sArePopupsEnabledForTesting;
        }

        AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
        if (delegate == null) {
            return false;
        }

        ActivityManager am =
                ContextUtils.getApplicationContext().getSystemService(ActivityManager.class);
        return delegate.isTaskMoveAllowedOnDisplay(am, display.getDisplayId());
    }

    /**
     * Adjusts window bounds of given {@link ChromeActivity} to match requested {@link
     * WindowFeatures} using at most one {@link android.app.ActivityManager.AppTask#moveTaskTo} call
     * on best-effort basis.
     */
    public static void adjustWindowBoundsToRequested(
            ChromeActivity activity, @Nullable WindowFeatures requestedWindowFeaturesDp) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.BAKLAVA) {
            return;
        }

        if (requestedWindowFeaturesDp == null) {
            Log.v(TAG, "adjustWindowBounds: requestedWindowFeaturesDp is null -- bailing out");
            return;
        }

        final WebContents webContents = activity.getActivityTab().getWebContents();
        if (webContents == null) {
            Log.w(TAG, "adjustWindowBounds: webContents is null -- bailing out");
            return;
        }

        final int realViewportWidthDp = webContents.getWidth();
        final int realViewportHeightDp = webContents.getHeight();
        final Rect realWindowBoundsPx =
                new Rect(activity.getWindowManager().getCurrentWindowMetrics().getBounds());
        final Rect targetWindowBoundsPx = new Rect(realWindowBoundsPx);
        Log.v(
                TAG,
                "adjustWindowBounds: current viewport size = ("
                        + realViewportWidthDp
                        + " x "
                        + realViewportHeightDp
                        + ") [dp]");
        Log.v(TAG, "adjustWindowBounds: top-level window bounds = " + realWindowBoundsPx + " [px]");
        Log.v(
                TAG,
                "adjustWindowBounds: requested WindowFeatures = "
                        + requestedWindowFeaturesDp
                        + " [dp]");

        if (requestedWindowFeaturesDp.width != null && requestedWindowFeaturesDp.height != null) {
            final WindowAndroid windowAndroid = activity.getWindowAndroid();
            if (windowAndroid == null) {
                Log.w(
                        TAG,
                        "adjustWindowBounds: activity.getWindowAndroid() is null -- bailing out");
                return;
            }

            DisplayAndroid display = windowAndroid.getDisplay();

            final int widthDiffDp = requestedWindowFeaturesDp.width - realViewportWidthDp;
            final int heightDiffDp = requestedWindowFeaturesDp.height - realViewportHeightDp;

            final int widthDiffPx = DisplayUtil.dpToPx(display, widthDiffDp);
            final int heightDiffPx = DisplayUtil.dpToPx(display, heightDiffDp);

            targetWindowBoundsPx.right += widthDiffPx;
            targetWindowBoundsPx.bottom += heightDiffPx;

            // TODO(https://crbug.com/411002260): detect if the popup has been opened cross-display
            // when Android display topology API is available in Chrome

            // If the display's dipScale is less than 1 it may happen that the difference in dps is
            // non-zero while the same difference in px is zero. In such case we consider it a
            // success as we could not have done anything better than being pixel perfect.

            RecordHistogram.recordBooleanHistogram(
                    "Android.MultiWindowMode.PopupBoundsAdjustment.DeltaWidth.Outcome.InDisplay",
                    widthDiffPx != 0);
            if (widthDiffPx != 0) {
                RecordHistogram.recordCount1000Histogram(
                        "Android.MultiWindowMode.PopupBoundsAdjustment.DeltaWidth.Positive.InDisplay",
                        Math.abs(widthDiffDp));
            }

            RecordHistogram.recordBooleanHistogram(
                    "Android.MultiWindowMode.PopupBoundsAdjustment.DeltaHeight.Outcome.InDisplay",
                    heightDiffPx != 0);
            if (heightDiffPx != 0) {
                RecordHistogram.recordCount1000Histogram(
                        "Android.MultiWindowMode.PopupBoundsAdjustment.DeltaHeight.Positive.InDisplay",
                        Math.abs(heightDiffDp));
            }

            Log.v(TAG, "adjustWindowBounds: widthDiffDp = " + widthDiffDp + " dp");
            Log.v(TAG, "adjustWindowBounds: widthDiffPx = " + widthDiffPx + " px");
            Log.v(TAG, "adjustWindowBounds: heightDiffDp = " + heightDiffDp + " dp");
            Log.v(TAG, "adjustWindowBounds: heightDiffPx = " + heightDiffPx + " px");
            Log.v(TAG, "adjustWindowBounds: display.getDipScale() = " + display.getDipScale());

            final InsetObserver insetObserver = windowAndroid.getInsetObserver();
            Insets windowInsetsOnSourceDisplay = Insets.NONE;
            if (insetObserver != null && insetObserver.getLastRawWindowInsets() != null) {
                windowInsetsOnSourceDisplay =
                        insetObserver
                                .getLastRawWindowInsets()
                                .getInsets(WindowInsetsCompat.Type.captionBar());
            }
            Log.v(
                    TAG,
                    "adjustWindowBounds: current window insets = "
                            + windowInsetsOnSourceDisplay
                            + " [px]");
            Log.v(
                    TAG,
                    "adjustWindowBounds: getTopControlsHeight from activity's BCM = "
                            + activity.getBrowserControlsManager().getTopControlsHeight()
                            + " px");
            Log.v(
                    TAG,
                    "adjustWindowBounds: getTopControlsHairlineHeight from activity's BCM = "
                            + activity.getBrowserControlsManager().getTopControlsHairlineHeight()
                            + " px");
        }

        if (realWindowBoundsPx.equals(targetWindowBoundsPx)) {
            Log.v(TAG, "adjustWindowBounds: perfect match -- no repositioning required");
            return;
        }
        Log.v(
                TAG,
                "adjustWindowBounds: repositioning required -- current bounds = "
                        + realWindowBoundsPx
                        + ", target bounds = "
                        + targetWindowBoundsPx);

        final AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
        if (delegate == null) {
            Log.w(TAG, "adjustWindowBounds: AconfigFlaggedApiDelegate is null -- bailing out");
            return;
        }

        final AppTask appTask = AndroidTaskUtils.getAppTaskFromId(activity, activity.getTaskId());
        if (appTask == null) {
            Log.w(TAG, "adjustWindowBounds: cannot find the AppTask -- bailing out");
            return;
        }

        Log.v(TAG, "adjustWindowBounds: dispatching the moveTaskTo call");
        delegate.moveTaskTo(appTask, INVALID_DISPLAY /* current display */, targetWindowBoundsPx);
    }

    /**
     * Returns negative insets expected to be the difference between WebContents viewport and
     * system-level window of the browser.
     */
    public static Insets getPopupInsetsForecast(
            @Nullable WindowAndroid sourceWindow, DisplayAndroid targetDisplay) {
        if (sInsetsForecastForTesting != null) {
            return sInsetsForecastForTesting;
        }
        if (sourceWindow == null) {
            Log.w(TAG, "getPopupInsetsForecast: sourceWindow is null -- bailing out");
            return Insets.NONE;
        }

        final Context targetDisplayContext = targetDisplay.getWindowContext();
        if (targetDisplayContext == null) {
            return Insets.NONE;
        }

        final InsetObserver insetObserver = sourceWindow.getInsetObserver();
        Insets windowInsetsOnSourceDisplay = Insets.NONE;
        if (insetObserver != null && insetObserver.getLastRawWindowInsets() != null) {
            windowInsetsOnSourceDisplay =
                    insetObserver
                            .getLastRawWindowInsets()
                            .getInsets(WindowInsetsCompat.Type.captionBar());
        }
        Log.v(
                TAG,
                "getPopupInsetsForecast: windowInsetsOnSourceDisplay = "
                        + windowInsetsOnSourceDisplay
                        + " [px]");

        final float densityFactor =
                targetDisplay.getDipScale() / sourceWindow.getDisplay().getDipScale();
        final Insets forecastedWindowInsetsOnTargetDisplay =
                Insets.of(
                        0,
                        0,
                        Math.round(
                                (windowInsetsOnSourceDisplay.left
                                                + windowInsetsOnSourceDisplay.right)
                                        * densityFactor),
                        Math.round(
                                (windowInsetsOnSourceDisplay.top
                                                + windowInsetsOnSourceDisplay.bottom)
                                        * densityFactor));
        Log.v(
                TAG,
                "getPopupInsetsForecast: forecastedWindowInsetsOnTargetDisplay = "
                        + forecastedWindowInsetsOnTargetDisplay
                        + " [px]");

        final int totalTopControlsHeightPx =
                predictBrowserTopControlsTotalHeightPx(targetDisplayContext);
        Log.v(
                TAG,
                "getPopupInsetsForecast: totalTopControlsHeightPx = "
                        + totalTopControlsHeightPx
                        + " px");

        final Insets totalInsets =
                Insets.add(
                        forecastedWindowInsetsOnTargetDisplay,
                        Insets.of(0, 0, 0, totalTopControlsHeightPx));
        Log.v(TAG, "getPopupInsetsForecast: totalInsets = " + totalInsets + " [px]");
        final Insets invertedTotalInsets = Insets.subtract(Insets.NONE, totalInsets);
        return invertedTotalInsets;
    }

    /**
     * Returns a prediction of overall height in pixels of browser-owned UI elements of a popup
     * spawned on display with given context.
     */
    private static int predictBrowserTopControlsTotalHeightPx(Context targetDisplayContext) {
        final int customTabsHeaderHeightPx =
                targetDisplayContext
                        .getResources()
                        .getDimensionPixelSize(R.dimen.custom_tabs_control_container_height);
        final int toolbarHairlineHeightPx =
                targetDisplayContext
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_hairline_height);
        Log.v(
                TAG,
                "predictBrowserTopControlsTotalHeightPx: customTabsHeaderHeightPx = "
                        + customTabsHeaderHeightPx
                        + " px");
        Log.v(
                TAG,
                "predictBrowserTopControlsTotalHeightPx: toolbarHairlineHeightPx = "
                        + toolbarHairlineHeightPx
                        + " px");
        return customTabsHeaderHeightPx + toolbarHairlineHeightPx;
    }

    public static void setArePopupsEnabledForTesting(boolean value) {
        sArePopupsEnabledForTesting = value;
        ResettersForTesting.register(() -> sArePopupsEnabledForTesting = null);
    }

    public static void setReparentingTaskForTesting(ReparentingTask task) {
        sReparentingTaskForTesting = task;
        ResettersForTesting.register(() -> sReparentingTaskForTesting = null);
    }

    /**
     * Overrides values returned by {@link getPopupInsetsForecast}. Usually it is desired to pass
     * insets that are non-positive in all directions.
     */
    public static void setInsetsForecastForTesting(Insets insets) {
        sInsetsForecastForTesting = insets;
        ResettersForTesting.register(() -> sInsetsForecastForTesting = null);
    }

    private static Intent initializePopupIntent() {
        Intent intent = new Intent();
        intent.setClass(ContextUtils.getApplicationContext(), CustomTabActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.POPUP);

        return intent;
    }

    private static ActivityOptions createPopupActivityOptions(
            WindowFeatures windowFeatures, @Nullable WindowAndroid sourceWindow) {
        ActivityOptions activityOptions = ActivityOptions.makeBasic();

        final Pair<DisplayAndroid, Rect> localCoordinates =
                getLocalCoordinatesPxFromWindowFeatures(windowFeatures);

        if (localCoordinates != null) {
            final DisplayAndroid display = localCoordinates.first;
            Rect bounds = localCoordinates.second;

            Log.v(TAG, "createPopupActivityOptions: ideal bounds = " + bounds);

            if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS)) {
                Insets insets = getPopupInsetsForecast(sourceWindow, display);
                Log.v(TAG, "createPopupActivityOptions: apply insets = " + insets);
                bounds = WindowInsetsUtils.insetRectangle(bounds, insets);
            }

            bounds = DisplayUtil.clampWindowToDisplay(bounds, display);
            Log.v(TAG, "createPopupActivityOptions: clamped bounds = " + bounds);

            activityOptions.setLaunchDisplayId(display.getDisplayId());
            activityOptions.setLaunchBounds(bounds);
        }

        return activityOptions;
    }

    private static @Nullable Pair<DisplayAndroid, Rect> getLocalCoordinatesPxFromWindowFeatures(
            WindowFeatures windowFeatures) {
        if (windowFeatures.width == null || windowFeatures.height == null) {
            return null;
        }

        final int widthDp = windowFeatures.width;
        final int heightDp = windowFeatures.height;
        final int leftDp = windowFeatures.left == null ? 0 : windowFeatures.left;
        final int topDp = windowFeatures.top == null ? 0 : windowFeatures.top;

        final int rightDp = leftDp + widthDp;
        final int bottomDp = topDp + heightDp;

        return DisplayUtil.convertGlobalDipToLocalPxCoordinates(
                new Rect(leftDp, topDp, rightDp, bottomDp));
    }

    private static ReparentingTask getReparentingTask(Tab tab) {
        if (sReparentingTaskForTesting != null) {
            return sReparentingTaskForTesting;
        }

        return ReparentingTask.from(tab);
    }
}
