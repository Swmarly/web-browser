// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Typeface;
import android.text.Spannable;
import android.text.style.StyleSpan;

import androidx.annotation.CallSuper;
import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteUIContext;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatch.MatchClassification;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/** A class that handles base properties and model for most suggestions. */
@NullMarked
public abstract class BaseSuggestionViewProcessor implements SuggestionProcessor {
    protected final AutocompleteUIContext mUiContext;
    protected final Context mContext;
    protected final SuggestionHost mSuggestionHost;
    private final ActionChipsProcessor mActionChipsProcessor;
    private final @Nullable OmniboxImageSupplier mImageSupplier;
    private final int mDesiredFaviconWidthPx;
    private final int mDecorationImageSizePx;
    private final int mSuggestionSizePx;

    /**
     * @param uiContext Context object containing common UI dependencies.
     */
    public BaseSuggestionViewProcessor(AutocompleteUIContext uiContext) {
        mUiContext = uiContext;
        mContext = uiContext.context;
        mSuggestionHost = uiContext.host;
        mImageSupplier = uiContext.imageSupplier;
        mDesiredFaviconWidthPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_favicon_size);
        mDecorationImageSizePx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_decoration_image_size);
        mSuggestionSizePx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_content_height);
        mActionChipsProcessor = new ActionChipsProcessor(uiContext.host);
    }

    /**
     * @return The desired size of Omnibox suggestion favicon.
     */
    protected int getDesiredFaviconSize() {
        return mDesiredFaviconWidthPx;
    }

    /**
     * @return The size of suggestion decoration images in pixels.
     */
    protected int getDecorationImageSize() {
        return mDecorationImageSizePx;
    }

    /** Return whether this suggestion can host OmniboxAction chips. */
    protected boolean allowOmniboxActions() {
        return true;
    }

    @Override
    public int getMinimumViewHeight() {
        return mSuggestionSizePx;
    }

    /**
     * Retrieve fallback icon for a given suggestion. Must be completed synchromously.
     *
     * @param match AutocompleteMatch instance to retrieve fallback icon for
     * @return OmniboxDrawableState that can be immediately applied to suggestion view
     */
    protected OmniboxDrawableState getFallbackIcon(AutocompleteMatch match) {
        int icon =
                match.isSearchSuggestion()
                        ? R.drawable.ic_suggestion_magnifier
                        : R.drawable.ic_globe_24dp;
        return OmniboxDrawableState.forSmallIcon(mContext, icon, true);
    }

    /**
     * Specify OmniboxDrawableState for suggestion decoration.
     *
     * @param model the PropertyModel to apply the decoration to
     * @param decoration the OmniboxDrawableState to apply
     */
    protected void setOmniboxDrawableState(
            PropertyModel model, @Nullable OmniboxDrawableState decoration) {
        model.set(BaseSuggestionViewProperties.ICON, decoration);
    }

    /**
     * Specify OmniboxDrawableState for action button.
     *
     * @param model Property model to update.
     * @param actions List of actions for the suggestion.
     */
    protected void setActionButtons(PropertyModel model, @Nullable List<Action> actions) {
        model.set(BaseSuggestionViewProperties.ACTION_BUTTONS, actions);
    }

    /**
     * Setup action icon as query build arrow.
     *
     * @param model Property model to update.
     * @param input The input to produce this suggestion.
     * @param suggestion Suggestion associated with the action button.
     * @param position The position of the button in the list.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void setRefineAction(
            PropertyModel model,
            AutocompleteInput input,
            AutocompleteMatch suggestion,
            int position) {
        if (suggestion.hasTabMatch() || suggestion.getType() == OmniboxSuggestionType.OPEN_TAB) {
            return;
        }

        String iconString =
                OmniboxResourceProvider.getString(
                        mContext,
                        R.string.accessibility_omnibox_btn_refine,
                        suggestion.getFillIntoEdit());
        @ControlsPosition Integer toolbarPosition = mUiContext.toolbarPositionSupplier.get();
        assumeNonNull(toolbarPosition);
        @DrawableRes
        int icon =
                toolbarPosition == ControlsPosition.TOP
                        ? R.drawable.btn_suggestion_refine_up
                        : R.drawable.btn_suggestion_refine_down;

        Runnable action =
                () -> {
                    if (suggestion.isSearchSuggestion()) {
                        RecordUserAction.record("MobileOmniboxRefineSuggestion.Search");
                    } else {
                        RecordUserAction.record("MobileOmniboxRefineSuggestion.Url");
                    }
                    mSuggestionHost.onRefineSuggestion(suggestion);
                };
        setActionButtons(
                model,
                List.of(
                        new Action(
                                OmniboxDrawableState.forSmallIcon(mContext, icon, true),
                                iconString,
                                action)));
    }

    /**
     * Process the click event.
     *
     * @param suggestion Selected suggestion.
     * @param position Position of the suggestion on the list.
     */
    protected void onSuggestionClicked(AutocompleteMatch suggestion, int position) {
        mSuggestionHost.onSuggestionClicked(suggestion, position, suggestion.getUrl());
    }

    /**
     * Process the long-click event.
     *
     * @param suggestion Selected suggestion.
     */
    protected void onSuggestionLongClicked(AutocompleteMatch suggestion) {
        mSuggestionHost.onDeleteMatch(suggestion, suggestion.getDisplayText());
    }

    /**
     * Process the touch down event. Only handles search suggestions.
     *
     * @param suggestion Selected suggestion.
     * @param position Position of the suggesiton on the list.
     */
    protected void onSuggestionTouchDownEvent(AutocompleteMatch suggestion, int position) {
        try (TimingMetric metric = OmniboxMetrics.recordTouchDownProcessTime()) {
            mSuggestionHost.onSuggestionTouchDown(suggestion, position);
        }
    }

    @Override
    public void populateModel(
            AutocompleteInput input,
            AutocompleteMatch suggestion,
            PropertyModel model,
            int position) {
        model.set(
                BaseSuggestionViewProperties.ON_CLICK,
                () -> onSuggestionClicked(suggestion, position));
        model.set(
                BaseSuggestionViewProperties.ON_LONG_CLICK,
                () -> onSuggestionLongClicked(suggestion));
        model.set(
                BaseSuggestionViewProperties.ON_FOCUS_VIA_SELECTION,
                () -> mSuggestionHost.setOmniboxEditingText(suggestion.getFillIntoEdit()));
        setActionButtons(model, null);

        model.set(BaseSuggestionViewProperties.USE_LARGE_DECORATION, false);
        model.set(BaseSuggestionViewProperties.SHOW_DECORATION, true);
        model.set(
                BaseSuggestionViewProperties.ACTION_CHIP_LEAD_IN_SPACING,
                OmniboxResourceProvider.getSuggestionDecorationIconSizeWidth(mContext));
        model.set(BaseSuggestionViewProperties.TOP_PADDING, 0);

        if (OmniboxFeatures.isTouchDownTriggerForPrefetchEnabled()
                && !OmniboxFeatures.isLowMemoryDevice()
                && suggestion.isSearchSuggestion()) {
            model.set(
                    BaseSuggestionViewProperties.ON_TOUCH_DOWN_EVENT,
                    () -> onSuggestionTouchDownEvent(suggestion, position));
        }

        if (allowOmniboxActions()) {
            mActionChipsProcessor.populateModel(suggestion, model, position);
        }

        var icon = getFallbackIcon(suggestion);
        assert icon != null;
        setOmniboxDrawableState(model, icon);
        if (suggestion.isSearchSuggestion()) {
            fetchImage(model, suggestion.getImageUrl());
        }

        // Action button should not be provided in the hub.
        if (input.getPageClassification() != PageClassification.ANDROID_HUB_VALUE) {
            addActionButtonIfAvailable(suggestion, model, position);
        }
    }

    private void addActionButtonIfAvailable(
            AutocompleteMatch suggestion, PropertyModel model, int position) {
        for (var action : suggestion.getActions()) {
            if (!action.showAsActionButton) {
                continue;
            }
            setActionButtons(
                    model,
                    List.of(
                            new Action(
                                    OmniboxDrawableState.forSmallIconWithIncognitoVariant(
                                            mContext,
                                            action.icon.buttonIconRes,
                                            action.icon.incognitoButtonIconRes,
                                            action.icon.tintWithTextColor),
                                    action.accessibilityHint,
                                    null,
                                    () -> {
                                        mSuggestionHost.onOmniboxActionClicked(action, position);
                                    })));
            // Only one action button is supported.
            return;
        }
    }

    @Override
    @CallSuper
    public void onOmniboxSessionStateChange(boolean activated) {}

    @Override
    @CallSuper
    public void onSuggestionsReceived() {}

    /**
     * Apply In-Place highlight to matching sections of Suggestion text.
     *
     * @param text Suggestion text to apply highlight to.
     * @param classifications Classifications describing how to format text.
     * @return true, if at least one highlighted match section was found.
     */
    protected static boolean applyHighlightToMatchRegions(
            Spannable text, List<MatchClassification> classifications) {
        if (text == null || classifications == null) return false;

        boolean hasAtLeastOneMatch = false;
        for (int i = 0; i < classifications.size(); i++) {
            MatchClassification classification = classifications.get(i);
            if ((classification.style & MatchClassificationStyle.MATCH)
                    == MatchClassificationStyle.MATCH) {
                int matchStartIndex = classification.offset;
                int matchEndIndex;
                if (i == classifications.size() - 1) {
                    matchEndIndex = text.length();
                } else {
                    matchEndIndex = classifications.get(i + 1).offset;
                }
                matchStartIndex = Math.min(matchStartIndex, text.length());
                matchEndIndex = Math.min(matchEndIndex, text.length());

                hasAtLeastOneMatch = true;
                // Bold the part of the URL that matches the user query.
                text.setSpan(
                        new StyleSpan(Typeface.BOLD),
                        matchStartIndex,
                        matchEndIndex,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        }
        return hasAtLeastOneMatch;
    }

    /**
     * Fetch suggestion favicon, if one is available. Updates icon decoration in supplied |model| if
     * |url| is not null and points to an already visited website.
     *
     * @param model Model representing current suggestion.
     * @param url Target URL the suggestion points to.
     */
    protected void fetchSuggestionFavicon(PropertyModel model, GURL url) {
        if (mImageSupplier != null) {
            mImageSupplier.fetchFavicon(
                    url,
                    icon -> {
                        if (icon != null) {
                            setOmniboxDrawableState(
                                    model, OmniboxDrawableState.forFavIcon(mContext, icon));
                        }
                    });
        }
    }

    /**
     * Fetch suggestion image. Updates icon decoration in supplied |model| if |imageUrl| is valid,
     * points to an image, and was successfully retrieved and decompressed.
     *
     * @param model the PropertyModel to update with retrieved image
     * @param imageUrl the URL of the image to retrieve and decode
     */
    protected void fetchImage(PropertyModel model, GURL imageUrl) {
        if (mImageSupplier != null) {
            mImageSupplier.fetchImage(
                    imageUrl,
                    bitmap -> {
                        if (bitmap != null) {
                            setOmniboxDrawableState(
                                    model, OmniboxDrawableState.forImage(mContext, bitmap));
                        }
                    });
        }
    }
}
