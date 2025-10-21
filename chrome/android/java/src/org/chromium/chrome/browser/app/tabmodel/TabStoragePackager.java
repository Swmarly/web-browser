// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.components.tabs.TabStripCollection;

import java.nio.ByteBuffer;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

/** Saves Java-accessible data for use in C++. */
@JNINamespace("tabs")
@NullMarked
public class TabStoragePackager {
    private final long mNativeTabStoragePackager;
    private final Map<TabStripCollection, TabModelInfo> mTabModelInfoMap = new HashMap<>();

    /** A data object representing a {@link TabModel} and its associated {@link WindowId}. */
    private static class TabModelInfo {
        public final @WindowId int windowId;
        public final @TabModelType int tabModelType;

        /**
         * @param windowId The {@link WindowId} the {@link TabModel} is associated with.
         * @param tabModelType The type of tab model being saved.
         */
        TabModelInfo(@WindowId int windowId, @TabModelType int tabModelType) {
            this.windowId = windowId;
            this.tabModelType = tabModelType;
        }

        /**
         * @param windowId The {@link WindowId} the {@link TabModel} is associated with.
         * @param isOffTheRecord Whether the tab model is off the record.
         */
        public static TabModelInfo createForWindowScopedModel(
                @WindowId int windowId, boolean isOffTheRecord) {
            return new TabModelInfo(
                    windowId, isOffTheRecord ? TabModelType.INCOGNITO : TabModelType.REGULAR);
        }

        /**
         * @param tabModel The {@link TabModel} associated with the {@link
         *     ArchivedTabModelOrchestrator}.
         */
        public static TabModelInfo createForArchivedModel() {
            return new TabModelInfo(TabWindowManager.INVALID_WINDOW_ID, TabModelType.ARCHIVED);
        }
    }

    private TabStoragePackager(long nativeTabStoragePackager) {
        mNativeTabStoragePackager = nativeTabStoragePackager;
    }

    @CalledByNative
    private static TabStoragePackager create(long nativeTabStoragePackager) {
        return new TabStoragePackager(nativeTabStoragePackager);
    }

    @CalledByNative
    public long packageTab(@JniType("const TabAndroid*") Tab tab) {
        WebContentsState state = TabStateExtractor.getWebContentsState(tab);
        return TabStoragePackagerJni.get()
                .consolidateTabData(
                        mNativeTabStoragePackager,
                        tab.getTimestampMillis(),
                        state == null ? null : state.buffer(),
                        assumeNonNull(TabAssociatedApp.getAppId(tab)),
                        tab.getThemeColor(),
                        tab.getLastNavigationCommittedTimestampMillis(),
                        tab.getTabHasSensitiveContent(),
                        tab);
    }

    private TabModelInfo getTabModelInfo(Profile profile, TabStripCollection collection) {
        if (mTabModelInfoMap.containsKey(collection)) {
            return mTabModelInfoMap.get(collection);
        }

        TabModelInfo info = getArchivedModelInfo(profile, collection);
        if (info == null) info = getWindowScopedModelInfo(collection);

        mTabModelInfoMap.put(collection, info);
        return info;
    }

    private TabModelInfo getWindowScopedModelInfo(TabStripCollection collection) {
        TabModel tabModel = null;
        TabModelSelector selector = null;
        Collection<TabModelSelector> selectors =
                TabWindowManagerSingleton.getInstance().getAllTabModelSelectors();
        for (TabModelSelector currentSelector : selectors) {
            tabModel = currentSelector.getTabModelForTabStripCollection(collection);
            if (tabModel != null) {
                selector = currentSelector;
                break;
            }
        }
        assert tabModel != null && selector != null;

        @WindowId
        int windowId = TabWindowManagerSingleton.getInstance().getWindowIdForSelector(selector);
        assert windowId != TabWindowManager.INVALID_WINDOW_ID;

        return TabModelInfo.createForWindowScopedModel(windowId, tabModel.isOffTheRecord());
    }

    @Nullable
    private TabModelInfo getArchivedModelInfo(Profile profile, TabStripCollection collection) {
        ArchivedTabModelOrchestrator orchestrator =
                ArchivedTabModelOrchestrator.getForProfile(profile);
        if (orchestrator == null) return null;

        TabModel tabModel = orchestrator.getTabModel();
        if (tabModel == null) return null;

        TabStripCollection archivedCollection = tabModel.getTabStripCollection();
        if (!Objects.equals(archivedCollection, collection)) return null;

        return TabModelInfo.createForArchivedModel();
    }

    @CalledByNative
    public long packageTabStripCollection(
            @JniType("Profile*") Profile profile,
            @JniType("const TabStripCollection*") TabStripCollection collection) {
        TabModelInfo info = getTabModelInfo(profile, collection);
        return TabStoragePackagerJni.get()
                .consolidateTabStripCollectionData(
                        mNativeTabStoragePackager, info.windowId, info.tabModelType);
    }

    @NativeMethods
    interface Natives {
        long consolidateTabData(
                long nativeTabStoragePackagerAndroid,
                long timestampMillis,
                @Nullable ByteBuffer webContentsStateBuffer,
                @Nullable @JniType("std::string") String openerAppId,
                int themeColor,
                long lastNavigationCommittedTimestampMillis,
                boolean tabHasSensitiveContent,
                @JniType("TabAndroid*") Tab tab);

        long consolidateTabStripCollectionData(
                long nativeTabStoragePackagerAndroid, int windowId, @TabModelType int tabModelType);
    }
}
