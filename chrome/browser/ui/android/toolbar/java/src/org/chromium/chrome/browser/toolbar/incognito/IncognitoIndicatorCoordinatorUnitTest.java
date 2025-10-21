// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.incognito;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;
import android.view.ViewStub;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;

/** Unit tests for {@link IncognitoIndicatorCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
public class IncognitoIndicatorCoordinatorUnitTest {
    private static final int BUTTON_WIDTH = 40;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ToolbarLayout mParentToolbar;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private ViewStub mIncognitoIndicatorStub;
    @Mock private View mIncognitoIndicatorView;
    @Mock private Context mContext;
    @Mock private Resources mResources;

    private IncognitoIndicatorCoordinator mCoordinator;

    @Before
    public void setUp() {
        when(mParentToolbar.findViewById(eq(R.id.incognito_indicator_stub)))
                .thenReturn(mIncognitoIndicatorStub);
        when(mIncognitoIndicatorStub.inflate()).thenReturn(mIncognitoIndicatorView);
        when(mParentToolbar.getContext()).thenReturn(mContext);
        when(mContext.getResources()).thenReturn(mResources);
        when(mResources.getDimensionPixelSize(anyInt())).thenReturn(BUTTON_WIDTH);

        mCoordinator =
                new IncognitoIndicatorCoordinator(
                        mParentToolbar,
                        mThemeColorProvider,
                        mIncognitoStateProvider,
                        /* visible= */ false);
        assertNull(
                "Indicator should not be inflated initially.",
                mCoordinator.getIncognitoIndicatorView());
    }

    @Test
    public void testOnIncognitoStateChanged_TogglesVisibility() {
        mCoordinator.setVisibility(/* visible= */ true);

        // Start not in incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ false);
        assertNull(
                "Indicator should not be inflated when not in incognito.",
                mCoordinator.getIncognitoIndicatorView());

        // Transition to incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ true);
        assertNotNull("Indicator should be inflated.", mCoordinator.getIncognitoIndicatorView());
        verify(mIncognitoIndicatorView).setVisibility(View.VISIBLE);
        clearInvocations(mIncognitoIndicatorView);

        // Transition back out of incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ false);
        verify(mIncognitoIndicatorView).setVisibility(View.GONE);
    }

    @Test
    public void testSetVisibility_TogglesVisibility() {
        // Start in incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ true);
        assertNotNull("Indicator should be inflated.", mCoordinator.getIncognitoIndicatorView());
        verify(mIncognitoIndicatorView).setVisibility(View.GONE);
        clearInvocations(mIncognitoIndicatorView);

        // Show toolbar buttons.
        mCoordinator.setVisibility(/* visible= */ true);
        assertNotNull("Indicator should be inflated.", mCoordinator.getIncognitoIndicatorView());
        verify(mIncognitoIndicatorView).setVisibility(View.VISIBLE);
        clearInvocations(mIncognitoIndicatorView);

        // Hide toolbar buttons.
        mCoordinator.setVisibility(/* visible= */ false);
        verify(mIncognitoIndicatorView).setVisibility(View.GONE);
        clearInvocations(mIncognitoIndicatorView);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    public void testUpdateVisibility_TabStripMigrationDisabled() {
        assertEquals(0, mCoordinator.updateVisibility(300));
        assertNull("Indicator should not be inflated.", mCoordinator.getIncognitoIndicatorView());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testUpdateVisibility_ToggleIncognito() {
        doReturn(0).when(mIncognitoIndicatorView).getMeasuredWidth();

        // Start not in incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ false);
        assertEquals(0, mCoordinator.updateVisibility(500));
        assertNull("Indicator should not be inflated.", mCoordinator.getIncognitoIndicatorView());

        // Switch to incognito.
        mCoordinator.onIncognitoStateChanged(/* isIncognito= */ true);
        assertEquals(
                "The coordinator should have consumed 3 times the button width by default.",
                120,
                mCoordinator.updateVisibility(500));
        assertNotNull("Indicator should be inflated.", mCoordinator.getIncognitoIndicatorView());
        verify(mIncognitoIndicatorView).setVisibility(View.VISIBLE);
        assertTrue(mCoordinator.needsUpdateBeforeShowing());
        clearInvocations(mIncognitoIndicatorView);

        assertEquals(
                "The coordinator should still consume 3 times the button width.",
                120,
                mCoordinator.updateVisibility(500));
        verify(mIncognitoIndicatorView).setVisibility(View.VISIBLE);
        assertTrue(mCoordinator.needsUpdateBeforeShowing());
        clearInvocations(mIncognitoIndicatorView);

        // Update the indicator's width measured width.
        doReturn(100).when(mIncognitoIndicatorView).getMeasuredWidth();
        assertTrue(mCoordinator.needsUpdateBeforeShowing());

        assertEquals(
                "The coordinator should now consume the previously measured width of the"
                        + " indicator.",
                100,
                mCoordinator.updateVisibility(500));
        verify(mIncognitoIndicatorView).setVisibility(View.VISIBLE);
        assertFalse(mCoordinator.needsUpdateBeforeShowing());
        clearInvocations(mIncognitoIndicatorView);

        // Hide the indicator when there isn't enough available width.
        assertEquals(
                "The coordinator should consume the remaining width, but not show.",
                50,
                mCoordinator.updateVisibility(50));
        verify(mIncognitoIndicatorView).setVisibility(View.GONE);
        assertFalse(mCoordinator.needsUpdateBeforeShowing());
        clearInvocations(mIncognitoIndicatorView);
    }
}
