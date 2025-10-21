// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.chips;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.widget.LoadingView;

/** Tests for {@link ChipViewTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowView.class})
@LooperMode(LooperMode.Mode.LEGACY)
public final class ChipViewTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private ChipView mChipView;

    @Before
    public void setup() {
        // Disabling animations is necessary to avoid running into issues with
        // delayed hiding of loading views.
        LoadingView.setDisableAnimationForTest(true);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.getTheme().applyStyle(R.style.Theme_BrowserUI_DayNight, true);
        mChipView = new ChipView(mActivity, null);
    }

    @Test
    @SmallTest
    public void primaryTextView() {
        mChipView.getPrimaryTextView().setText("Primary text");

        TextView primaryText = mChipView.findViewById(R.id.chip_view_primary_text);
        assertNotNull(primaryText);
        assertEquals(View.VISIBLE, primaryText.getVisibility());
        assertEquals("Primary text", primaryText.getText());
    }

    @Test
    @SmallTest
    public void secondaryTextView() {
        mChipView.getSecondaryTextView().setText("Secondary text");

        TextView secondaryText = mChipView.findViewById(R.id.chip_view_secondary_text);
        assertNotNull(secondaryText);
        assertEquals(View.VISIBLE, secondaryText.getVisibility());
        assertEquals("Secondary text", secondaryText.getText());
    }

    @Test
    @SmallTest
    public void setTwoLineChip() {
        ChipView twoLineChip =
                (ChipView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.two_line_chip_view_test_item, null);
        twoLineChip.getPrimaryTextView().setText("Primary text");
        twoLineChip.getSecondaryTextView().setText("Secondary text");

        LinearLayout textWrapper = twoLineChip.findViewById(R.id.chip_view_text_wrapper);
        assertNotNull(textWrapper);
        assertEquals(LinearLayout.VERTICAL, textWrapper.getOrientation());
        assertEquals(2, textWrapper.getChildCount());

        // Default layout parameters used for vertically oriented linear layout are (MATCH_PARENT,
        // WRAP_CONTENT). Chip view isn't measured correctly with these layout parameters. For more
        // information, see crbug.com/450830784.
        assertEquals(
                LayoutParams.WRAP_CONTENT,
                twoLineChip.getPrimaryTextView().getLayoutParams().width);
        assertEquals(
                LayoutParams.WRAP_CONTENT,
                twoLineChip.getSecondaryTextView().getLayoutParams().width);
    }

    @Test
    @SmallTest
    public void loadingView() {
        // The start icon shouldn't be visible by default.
        ImageView startIcon = mChipView.findViewById(R.id.chip_view_start_icon);
        assertEquals(View.GONE, startIcon.getVisibility());

        // The start icon should become visible after it's set.
        mChipView.setIcon(R.drawable.ic_settings_gear_24dp, /* tintWithTextColor= */ false);
        assertEquals(View.VISIBLE, startIcon.getVisibility());

        LoadingView loadingView = mChipView.findViewById(R.id.chip_view_loading_view);
        assertEquals(View.GONE, loadingView.getVisibility());

        LoadingView.Observer firstObserver = mock(LoadingView.Observer.class);
        mChipView.showLoadingView(firstObserver);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(View.VISIBLE, loadingView.getVisibility());
        // The start icon shouldn't be visible when the loading view is displayed.
        assertEquals(View.GONE, startIcon.getVisibility());
        verify(firstObserver).onShowLoadingUiComplete();

        LoadingView.Observer secondObserver = mock(LoadingView.Observer.class);
        mChipView.hideLoadingView(secondObserver);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(View.GONE, loadingView.getVisibility());
        // The start icon should be visible again when the loading view becomes hidden.
        assertEquals(View.VISIBLE, startIcon.getVisibility());
        verify(secondObserver).onHideLoadingUiComplete();
    }

    @Test
    @SmallTest
    public void cancelButton() {
        assertNull(mChipView.findViewById(R.id.chip_view_end_icon));

        mChipView.addRemoveIcon();
        ImageView cancelButton = mChipView.findViewById(R.id.chip_view_end_icon);
        assertNotNull(cancelButton);
        assertEquals(View.VISIBLE, cancelButton.getVisibility());
        assertEquals(
                R.drawable.btn_close, shadowOf(cancelButton.getDrawable()).getCreatedFromResId());
    }

    @Test
    @SmallTest
    public void cancelButtonClickListener() {
        mChipView.addRemoveIcon();
        mChipView.getPrimaryTextView().setText("Primary text");

        OnClickListener onClickListener = mock(OnClickListener.class);
        mChipView.setRemoveIconClickListener(onClickListener);
        View cancelButtonWrapper = mChipView.findViewById(R.id.chip_cancel_btn);
        cancelButtonWrapper.performClick();
        verify(onClickListener).onClick(eq(cancelButtonWrapper));
    }

    @Test
    @SmallTest
    public void dropdownButton() {
        assertNull(mChipView.findViewById(R.id.chip_view_end_icon));

        mChipView.addDropdownIcon();
        ImageView dropdownButton = mChipView.findViewById(R.id.chip_view_end_icon);
        assertNotNull(dropdownButton);
        assertEquals(View.VISIBLE, dropdownButton.getVisibility());
        assertEquals(
                R.drawable.mtrl_dropdown_arrow,
                shadowOf(dropdownButton.getDrawable()).getCreatedFromResId());
    }

    @Test
    @SmallTest
    public void startIcon() {
        ImageView startIcon = mChipView.findViewById(R.id.chip_view_start_icon);
        assertEquals(View.GONE, startIcon.getVisibility());

        mChipView.setIcon(R.drawable.ic_settings_gear_24dp, /* tintWithTextColor= */ false);
        assertEquals(View.VISIBLE, startIcon.getVisibility());
    }
}
