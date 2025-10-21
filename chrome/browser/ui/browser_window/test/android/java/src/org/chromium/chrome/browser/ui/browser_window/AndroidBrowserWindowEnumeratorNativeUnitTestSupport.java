// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;

/** Supports {@code android_browser_window_enumerator_unittest.cc}. */
@NullMarked
final class AndroidBrowserWindowEnumeratorNativeUnitTestSupport {

    private AndroidBrowserWindowEnumeratorNativeUnitTestSupport() {}

    @CalledByNative
    private static long createBrowserWindow(int taskId, Profile profile) {
        var mockActivityWindowAndroid =
                ChromeAndroidTaskUnitTestSupport.createMockActivityWindowAndroid(taskId);
        var tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(profile);
        var chromeAndroidTask =
                ChromeAndroidTaskTrackerImpl.getInstance()
                        .obtainTask(
                                BrowserWindowType.NORMAL,
                                mockActivityWindowAndroid,
                                tabModel,
                                null);
        return chromeAndroidTask.getOrCreateNativeBrowserWindowPtr();
    }

    /**
     * This function simulates Android OS behavior to activate a browser window. It calls into a
     * task's onTopResumedActivityChangedWithNative() function to make sure its
     * mLastActivatedTimeMillis is positive, which is a requirement for being able to call into
     * ChromeAndroidTaskTrackerImpl.getNativeBrowserWindowPtrsOrderedByActivation(). Without this,
     * |mLastActivatedTimeMillis| will always be -1 (invalid value) as the window isn't activated.
     */
    @CalledByNative
    private static void activateBrowserWindow(int taskId) {
        ChromeAndroidTaskImpl task =
                (ChromeAndroidTaskImpl) ChromeAndroidTaskTrackerImpl.getInstance().get(taskId);
        assert task != null;
        task.onTopResumedActivityChangedWithNative(/* isTopResumedActivity= */ true);
    }

    @CalledByNative
    private static void destroyBrowserWindow(int taskId) {
        ChromeAndroidTaskTrackerImpl.getInstance().remove(taskId);
    }

    @CalledByNative
    private static void destroyAllBrowserWindows() {
        ChromeAndroidTaskTrackerImpl.getInstance().removeAllForTesting();
    }
}
