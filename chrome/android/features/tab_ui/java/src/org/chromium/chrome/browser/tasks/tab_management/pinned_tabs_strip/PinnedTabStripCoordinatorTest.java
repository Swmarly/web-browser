// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Unit tests for {@link PinnedTabStripCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PinnedTabStripCoordinatorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TabListCoordinator mTabListCoordinator;
    @Mock private TabListRecyclerView mTabGridListRecyclerView;
    @Mock private GridLayoutManager mLayoutManager;
    @Mock private PinnedTabStripMediator mMediator;
    @Mock private ObservableSupplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;

    private PinnedTabStripCoordinator mCoordinator;

    @Before
    public void setUp() {
        when(mTabListCoordinator.getContainerView()).thenReturn(mTabGridListRecyclerView);
        when(mTabGridListRecyclerView.getLayoutManager()).thenReturn(mLayoutManager);
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
    }

    private void onActivity(TestActivity activity) {
        FrameLayout parentView = new FrameLayout(activity);
        mCoordinator =
                new PinnedTabStripCoordinator(
                        activity, parentView, mTabListCoordinator, mTabGroupModelFilterSupplier) {
                    @Override
                    PinnedTabStripMediator createMediator(
                            Activity activity,
                            RecyclerView tabGridListRecyclerView,
                            TabListCoordinator tabListCoordinator,
                            TabListModel tabListModel,
                            TabListModel pinnedTabsModelList,
                            PropertyModel stripPropertyModel,
                            ObservableSupplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
                        return mMediator;
                    }
                };
    }

    @Test
    public void testSetsUpRecyclerView() {
        RecyclerView pinnedTabRecyclerView = mCoordinator.getPinnedTabsRecyclerView();
        assertThat(pinnedTabRecyclerView.getLayoutManager()).isNotNull();
        assertThat(pinnedTabRecyclerView.getAdapter()).isNotNull();

        assertTrue(pinnedTabRecyclerView.getAdapter() instanceof SimpleRecyclerViewAdapter);
        assertTrue(pinnedTabRecyclerView.getLayoutManager() instanceof LinearLayoutManager);

        Assert.assertEquals(
                LinearLayoutManager.HORIZONTAL,
                ((LinearLayoutManager) pinnedTabRecyclerView.getLayoutManager()).getOrientation());
    }

    @Test
    public void testScrollListener_onScrolled() {
        ArgumentCaptor<RecyclerView.OnScrollListener> scrollListenerCaptor =
                ArgumentCaptor.forClass(RecyclerView.OnScrollListener.class);
        verify(mTabGridListRecyclerView).addOnScrollListener(scrollListenerCaptor.capture());

        scrollListenerCaptor.getValue().onScrolled(mTabGridListRecyclerView, 0, 10);
        verify(mMediator).onScrolled();
    }

    @Test
    public void testDestroy() {
        mCoordinator.destroy();
        verify(mMediator).destroy();
    }
}
