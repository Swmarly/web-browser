// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static android.support.annotation.VisibleForTesting.PACKAGE_PRIVATE;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MVT;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.SINGLE_THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;

import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.StyleRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.browser.customtabs.CustomTabsIntent;

import com.google.android.material.color.DynamicColorsOptions;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.theme.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorFromHexInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;
import org.chromium.ui.util.ColorUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.concurrent.Executor;

/** Utility class of the NTP customization. */
@NullMarked
public class NtpCustomizationUtils {

    @IntDef({
        NtpBackgroundImageType.DEFAULT,
        NtpBackgroundImageType.IMAGE_FROM_DISK,
        NtpBackgroundImageType.CHROME_COLOR,
        NtpBackgroundImageType.THEME_COLLECTION,
        NtpBackgroundImageType.COLOR_FROM_HEX
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NtpBackgroundImageType {
        int DEFAULT = 0;
        int IMAGE_FROM_DISK = 1;
        int CHROME_COLOR = 2;
        int THEME_COLLECTION = 3;
        int COLOR_FROM_HEX = 4;
        int NUM_ENTRIES = 5;
    }

    @VisibleForTesting static final String NTP_BACKGROUND_IMAGE_FILE = "ntp_background_image";
    private static final String TAG = "NtpCustomization";

    /**
     * Every list in NTP customization bottom sheets should use this function to get the background
     * for its list items.
     *
     * @param size The number of the list items to be displayed in a container view.
     * @param index The index of the currently iterated list item.
     * @return The background of the list item view at the given index.
     */
    public static int getBackground(int size, int index) {
        if (size == 1) {
            return R.drawable.ntp_customization_bottom_sheet_list_item_background_single;
        }

        if (index == 0) {
            return R.drawable.ntp_customization_bottom_sheet_list_item_background_top;
        }

        if (index == size - 1) {
            return R.drawable.ntp_customization_bottom_sheet_list_item_background_bottom;
        }

        return R.drawable.ntp_customization_bottom_sheet_list_item_background_middle;
    }

    /**
     * Returns the resource ID of the content description for the bottom sheet. The main bottom
     * sheet's content description requires special handling beyond this function.
     */
    public static int getSheetContentDescription(
            @NtpCustomizationCoordinator.BottomSheetType int type) {
        switch (type) {
            case MAIN:
                return R.string.ntp_customization_main_bottom_sheet;
            case MVT:
                return R.string.ntp_customization_mvt_bottom_sheet;
            case NTP_CARDS:
                return R.string.ntp_customization_ntp_cards_bottom_sheet;
            case FEED:
                return R.string.ntp_customization_feed_bottom_sheet;
            case THEME:
                return R.string.ntp_customization_theme_bottom_sheet;
            case THEME_COLLECTIONS:
            case SINGLE_THEME_COLLECTION:
                return R.string.ntp_customization_theme_collections_bottom_sheet;
            case CHROME_COLORS:
                return R.string.ntp_customization_chrome_colors_bottom_sheet;
            default:
                assert false : "Bottom sheet type not supported!";
                return -1;
        }
    }

    /**
     * Returns the resource ID for the accessibility string announced when the bottom sheet is fully
     * expanded.
     */
    public static int getSheetFullHeightAccessibilityStringId(
            @NtpCustomizationCoordinator.BottomSheetType int type) {
        switch (type) {
            case MAIN:
                return R.string.ntp_customization_main_bottom_sheet_opened_full;
            case MVT:
                return R.string.ntp_customization_mvt_bottom_sheet_opened_full;
            case NTP_CARDS:
                return R.string.ntp_customization_ntp_cards_bottom_sheet_opened_full;
            case FEED:
                return R.string.ntp_customization_feed_bottom_sheet_opened_full;
            case THEME:
                return R.string.ntp_customization_theme_bottom_sheet_opened_full;
            case THEME_COLLECTIONS:
            case SINGLE_THEME_COLLECTION:
                return R.string.ntp_customization_theme_collections_bottom_sheet_opened_full;
            case CHROME_COLORS:
                return R.string.ntp_customization_chrome_colors_bottom_sheet_opened_full;
            default:
                assert false : "Bottom sheet type not supported!";
                return -1;
        }
    }

    /**
     * Returns the resource ID for the accessibility string announced when the bottom sheet is half
     * expanded.
     */
    public static int getSheetHalfHeightAccessibilityStringId(
            @NtpCustomizationCoordinator.BottomSheetType int type) {
        switch (type) {
            case MAIN:
                return R.string.ntp_customization_main_bottom_sheet_opened_half;
            case MVT:
                return R.string.ntp_customization_mvt_bottom_sheet_opened_half;
            case NTP_CARDS:
                return R.string.ntp_customization_ntp_cards_bottom_sheet_opened_half;
            case FEED:
                return R.string.ntp_customization_feed_bottom_sheet_opened_half;
            case THEME:
                return R.string.ntp_customization_theme_bottom_sheet_opened_half;
            case THEME_COLLECTIONS:
            case SINGLE_THEME_COLLECTION:
                return R.string.ntp_customization_theme_collections_bottom_sheet_opened_half;
            case CHROME_COLORS:
                return R.string.ntp_customization_chrome_colors_bottom_sheet_opened_half;
            default:
                assert false : "Bottom sheet type not supported!";
                return -1;
        }
    }

    /** Returns the customized primary color if set, null otherwise. */
    public @Nullable static @ColorInt Integer getPrimaryColorFromCustomizedThemeColor(
            Context context) {
        if (!ChromeFeatureList.sNewTabPageCustomizationV2.isEnabled()) return null;

        @NtpBackgroundImageType int imageType = getNtpBackgroundImageTypeFromSharedPreference();
        if (imageType == NtpBackgroundImageType.DEFAULT) {
            return null;
        }

        if (imageType == NtpBackgroundImageType.CHROME_COLOR) {
            @NtpThemeColorId int colorId = getNtpThemeColorIdFromSharedPreference();
            if (colorId == NtpThemeColorId.DEFAULT) return null;

            return NtpThemeColorUtils.getNtpThemePrimaryColor(context, colorId);
        }

        @ColorInt int color = getCustomizedPrimaryColorFromSharedPreference();
        return (color != NtpThemeColorInfo.COLOR_NOT_SET) ? color : null;
    }

    /** Loads the NtpThemeColorInfo from the SharedPreference, null otherwise. */
    public @Nullable static NtpThemeColorInfo loadColorInfoFromSharedPreference(Context context) {
        if (!ChromeFeatureList.sNewTabPageCustomizationV2.isEnabled()) return null;

        @NtpBackgroundImageType int imageType = getNtpBackgroundImageTypeFromSharedPreference();
        if (imageType == NtpBackgroundImageType.DEFAULT) {
            return null;
        }

        if (imageType == NtpBackgroundImageType.CHROME_COLOR) {
            // For CHROME_COLOR, a color resource id is saved in the SharedPreference.
            @NtpThemeColorId int colorId = getNtpThemeColorIdFromSharedPreference();
            if (colorId == NtpThemeColorId.DEFAULT) return null;

            return NtpThemeColorUtils.createNtpThemeColorInfo(context, colorId);
        }

        // For other types, a color value is saved in the SharedPreference.
        @ColorInt int primaryColor = getCustomizedPrimaryColorFromSharedPreference();
        if (primaryColor == NtpThemeColorInfo.COLOR_NOT_SET) return null;

        @ColorInt int backgroundColor = NtpThemeColorInfo.COLOR_NOT_SET;
        if (imageType == NtpBackgroundImageType.COLOR_FROM_HEX) {
            backgroundColor =
                    getBackgroundColorFromSharedPreference(NtpThemeColorInfo.COLOR_NOT_SET);
        }
        return new NtpThemeColorFromHexInfo(context, backgroundColor, primaryColor);
    }

    // Gets the content based primary color for a bitmap.
    public @Nullable static @ColorInt Integer getContentBasedSeedColor(Bitmap bitmap) {
        DynamicColorsOptions.Builder builder = new DynamicColorsOptions.Builder();
        builder.setContentBasedSource(bitmap);
        DynamicColorsOptions dynamicColorsOptions = builder.build();
        return dynamicColorsOptions.getContentBasedSeedColor();
    }

    // Launch a new activity in the same task with the given uri as a CCT.
    public static void launchUriActivity(Context context, String uri) {
        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.setShareState(CustomTabsIntent.SHARE_STATE_ON);
        Intent intent = builder.build().intent;
        intent.setPackage(context.getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        intent.setData(Uri.parse(uri));
        intent.setAction(Intent.ACTION_VIEW);
        intent.setClassName(context, "org.chromium.chrome.browser.customtabs.CustomTabActivity");
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        context.startActivity(intent);
    }

    /**
     * Sets the NTP's background image type to the SharedPreference.
     *
     * @param imageType The new image type.
     */
    public static void setNtpBackgroundImageTypeToSharedPreference(
            @NtpBackgroundImageType int imageType) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_IMAGE_TYPE, imageType);
    }

    /** Gets the current NTP's background image type from the SharedPreference. */
    public static @NtpBackgroundImageType int getNtpBackgroundImageTypeFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_IMAGE_TYPE,
                NtpBackgroundImageType.DEFAULT);
    }

    /**
     * Saves the background image if it isn't null, otherwise removes the file.
     *
     * @param backgroundImageBitmap The bitmap of the background image.
     */
    public static void updateBackgroundImageFile(@Nullable Bitmap backgroundImageBitmap) {
        if (backgroundImageBitmap == null) {
            deleteBackgroundImageFile();
            return;
        }

        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                saveBackgroundImageFile(backgroundImageBitmap);
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Saves the background transformation matrices to SharedPreferences.
     *
     * @param backgroundImageInfo The {@link BackgroundImageInfo} object containing the portrait and
     *     landscape matrices.
     */
    public static void updateBackgroundImageMatrices(BackgroundImageInfo backgroundImageInfo) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeString(
                ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_MATRIX,
                matrixToString(backgroundImageInfo.portraitMatrix));
        prefsManager.writeString(
                ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_MATRIX,
                matrixToString(backgroundImageInfo.landscapeMatrix));
    }

    /** Returns whether a white background should be applied on fake search box. */
    public static boolean shouldApplyWhiteBackgroundOnSearchBox() {
        if (!ChromeFeatureList.sNewTabPageCustomizationV2.isEnabled()) return false;

        return shouldApplyWhiteBackgroundOnSearchBox(
                NtpCustomizationConfigManager.getInstance().getBackgroundImageType());
    }

    /**
     * Returns whether a white background should be applied on fake search box based on the provided
     * background image type.
     */
    public static boolean shouldApplyWhiteBackgroundOnSearchBox(@NtpBackgroundImageType int type) {
        return type == NtpBackgroundImageType.IMAGE_FROM_DISK
                || type == NtpBackgroundImageType.THEME_COLLECTION;
    }

    @VisibleForTesting
    static void saveBackgroundImageFile(Bitmap backgroundImageBitmap) {
        File file = getBackgroundImageFile();

        try (FileOutputStream fileOutputStream = new FileOutputStream(file)) {
            backgroundImageBitmap.compress(Bitmap.CompressFormat.PNG, 100, fileOutputStream);
        } catch (IOException e) {
            Log.i(TAG, "Failed to save background image to: " + file.getAbsolutePath());
        }
    }

    /** Returns the file to save the NTP's background image. */
    @VisibleForTesting
    public static File getBackgroundImageFile() {
        return new File(
                ContextUtils.getApplicationContext().getFilesDir(), NTP_BACKGROUND_IMAGE_FILE);
    }

    @VisibleForTesting
    static void deleteBackgroundImageFile() {
        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                deleteBackgroundImageFileImpl();
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @VisibleForTesting
    static void deleteBackgroundImageFileImpl() {
        File file = getBackgroundImageFile();
        if (file.exists()) {
            file.delete();
        }
    }

    /**
     * Loads the NTP's background bitmap image from disk.
     *
     * @param callback The callback to notice when the image is loaded.
     * @param executor The executor for the loading task.
     */
    public static void readNtpBackgroundImage(
            Callback<@Nullable Bitmap> callback, Executor executor) {
        new AsyncTask<Bitmap>() {
            @Override
            // The return value of the super class doesn't have @Nullable annotation.
            @SuppressWarnings("NullAway")
            protected Bitmap doInBackground() {
                return readNtpBackgroundImageImpl();
            }

            @Override
            protected void onPostExecute(Bitmap bitmap) {
                if (bitmap == null) {
                    callback.onResult(null);
                    return;
                }
                callback.onResult(bitmap);
            }
        }.executeOnExecutor(executor);
    }

    @VisibleForTesting
    static @Nullable Bitmap readNtpBackgroundImageImpl() {
        File file = getBackgroundImageFile();

        if (!file.exists()) {
            return null;
        }

        return BitmapFactory.decodeFile(file.getPath(), null);
    }

    /** Loads the NTP's background transformation matrices from SharedPreferences. */
    public static @Nullable BackgroundImageInfo readNtpBackgroundImageMatrices() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        String portraitMatrixString =
                prefsManager.readString(
                        ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_MATRIX, null);
        String landscapeMatrixString =
                prefsManager.readString(
                        ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_MATRIX, null);

        if (TextUtils.isEmpty(portraitMatrixString) || TextUtils.isEmpty(landscapeMatrixString)) {
            return null;
        }

        Matrix portraitMatrix = stringToMatrix(portraitMatrixString);
        Matrix landscapeMatrix = stringToMatrix(landscapeMatrixString);

        if (portraitMatrix == null || landscapeMatrix == null) {
            return null;
        }

        return new BackgroundImageInfo(portraitMatrix, landscapeMatrix);
    }

    /** Converts a Matrix into a string representation for storage. */
    public static String matrixToString(Matrix matrix) {
        float[] values = new float[9];
        matrix.getValues(values);
        return Arrays.toString(values);
    }

    /** Converts a string representation back into a Matrix. Returns null on failure. */
    public static @Nullable Matrix stringToMatrix(String matrixString) {
        if (matrixString == null || !matrixString.startsWith("[") || !matrixString.endsWith("]")) {
            return null;
        }

        try {
            // Remove brackets and spaces
            String[] stringValues =
                    matrixString.substring(1, matrixString.length() - 1).split(", ");
            if (stringValues.length != 9) return null;

            float[] values = new float[9];
            for (int i = 0; i < 9; i++) {
                values[i] = Float.parseFloat(stringValues[i]);
            }

            Matrix matrix = new Matrix();
            matrix.setValues(values);
            return matrix;
        } catch (Exception e) {
            Log.i(TAG, "Error in stringToMatrix: " + e);
            return null;
        }
    }

    /**
     * Sets the NTP's background color to the SharedPreference.
     *
     * @param color The new background color.
     */
    public static void setBackgroundColorToSharedPreference(@ColorInt int color) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR, color);
    }

    /** Gets the NTP's background color from the SharedPreference. */
    public static @ColorInt int getBackgroundColorFromSharedPreference(@ColorInt int defaultColor) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR, defaultColor);
    }

    /**
     * Sets the NTP's color theme id to the SharedPreference.
     *
     * @param themeColorId The new color theme id.
     */
    @VisibleForTesting(otherwise = PACKAGE_PRIVATE)
    public static void setNtpThemeColorIdToSharedPreference(@NtpThemeColorId int themeColorId) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID, themeColorId);
    }

    /** Gets the NTP's color theme id from the SharedPreference. */
    public static @NtpThemeColorId int getNtpThemeColorIdFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID, NtpThemeColorId.DEFAULT);
    }

    /**
     * Sets the customized primary color to the SharedPreference.
     *
     * @param color The new primary theme color.
     */
    public static void setCustomizedPrimaryColorToSharedPreference(@ColorInt int color) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR, color);
    }

    /** Gets the customized primary color from the SharedPreference. */
    public static @ColorInt int getCustomizedPrimaryColorFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR,
                NtpThemeColorInfo.COLOR_NOT_SET);
    }

    /**
     * Returns an instance of ColorStateList which is used to tint icon buttons.
     *
     * @param context Used to get the ColorStateList.
     */
    public static @Nullable ColorStateList getSearchBoxIconColorTint(Context context) {
        return getSearchBoxIconColorTint(context, shouldApplyWhiteBackgroundOnSearchBox());
    }

    /**
     * Returns an instance of ColorStateList which is used to tint icon buttons based on the flag of
     * whether a white background will be applied.
     *
     * @param context Used to get the ColorStateList.
     * @param shouldApplyWhiteBackgroundOnSearchBox Whether a white background will be applied.
     */
    public static @Nullable ColorStateList getSearchBoxIconColorTint(
            Context context, boolean shouldApplyWhiteBackgroundOnSearchBox) {
        if (shouldApplyWhiteBackgroundOnSearchBox) {
            return AppCompatResources.getColorStateList(context, R.color.default_icon_color_dark);
        }

        return ThemeUtils.getThemedToolbarIconTint(context, BrandedColorScheme.APP_DEFAULT);
    }

    /**
     * Returns the text appearance resource id based on a flag of whether a white background will be
     * applied.
     */
    public static @StyleRes int getSearchBoxTextStyleResId(
            boolean shouldApplyWhiteBackgroundOnSearchBox) {
        if (shouldApplyWhiteBackgroundOnSearchBox) {
            return R.style.TextAppearance_ComposeplateTextMediumDark;
        }

        return R.style.TextAppearance_ComposeplateTextMedium;
    }

    /** Removes the NTP's background color key and primary color key from the SharedPreference. */
    static void resetCustomizedColors() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_IMAGE_TYPE);
    }

    /** Returns whether all flags are enabled to allow edge-to-edge for customized theme. */
    public static boolean canEnableEdgeToEdgeForCustomizedTheme(
            WindowAndroid windowAndroid, boolean isTablet) {
        return canEnableEdgeToEdgeForCustomizedTheme(isTablet)
                && EdgeToEdgeStateProvider.isEdgeToEdgeEnabledForWindow(windowAndroid);
    }

    /**
     * Returns whether all flags are enabled to allow edge-to-edge for customized theme. This method
     * doesn't check EdgeToEdgeStateProvider.
     */
    public static boolean canEnableEdgeToEdgeForCustomizedTheme(boolean isTablet) {
        return !isTablet
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && ChromeFeatureList.sNewTabPageCustomizationV2.isEnabled();
    }

    /**
     * Returns whether the given Tab supports to remove the top Status bar to make it truly edge to
     * edge.
     */
    public static boolean supportsEnableEdgeToEdgeOnTop(@Nullable Tab tab) {
        if (tab == null || !tab.isNativePage()) {
            return false;
        }

        return assumeNonNull(tab.getNativePage()).supportsEdgeToEdgeOnTop();
    }

    /**
     * Returns whether to skip a layout change from the given systemTopInset and consumeTopInset
     * status.
     *
     * @param appliedTopPadding The value of currently applied top padding.
     * @param systemTopInset The system's top inset, i.e., the height of Status bar.
     * @param consumeTopInset Whether should consume the system's top inset.
     */
    public static boolean shouldSkipTopInsetsChange(
            int appliedTopPadding, int systemTopInset, boolean consumeTopInset) {
        // We skip a layout change if the top padding doesn't need adjusting. This occurs in two
        // scenarios:
        // 1) Top padding was already added and should remain.
        // 2) No top padding was added and none is needed now.
        return ((appliedTopPadding == systemTopInset) && consumeTopInset)
                || ((appliedTopPadding == 0) && !consumeTopInset);
    }

    /**
     * Sets tint color for the default Google logo.
     *
     * @param context Used to look up current day/night mode status.
     */
    public static void setTintForDefaultGoogleLogo(
            Context context, Drawable defaultGoogleLogoDrawable) {
        // Check the mode before applying a tinted color. A transparent tint in light mode will
        // cause the logo's color to disappear.
        boolean isNightMode = ColorUtils.inNightMode(context);
        @NtpBackgroundImageType
        int defaultBackgroundType =
                NtpCustomizationConfigManager.getInstance().getBackgroundImageType();

        // The colorful Google logo is shown for default theme in light mode.
        if (!isNightMode && defaultBackgroundType == NtpBackgroundImageType.DEFAULT) {
            return;
        }

        @ColorInt int tintColor;
        if (defaultBackgroundType == NtpBackgroundImageType.CHROME_COLOR
                || defaultBackgroundType == NtpBackgroundImageType.COLOR_FROM_HEX) {
            @ColorInt
            Integer primaryColor =
                    NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(context);
            if (primaryColor != null) {
                tintColor = primaryColor.intValue();
            } else if (!isNightMode) {
                // When primary color is missing, falls back to colorful Google logo in light mode.
                return;
            } else {
                // When primary color is missing, falls back to white Google logo in light mode.
                tintColor = Color.WHITE;
            }
        } else {
            // For all other cases, white color is used. This includes: Ntps with a customized
            // background image in either light or dark mode; or Ntps without any theme in dark
            // mode.
            tintColor = Color.WHITE;
        }

        defaultGoogleLogoDrawable.setTint(tintColor);
    }

    public static void resetSharedPreferenceForTesting() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_IMAGE_TYPE);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_MATRIX);
        prefsManager.removeKey(ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_MATRIX);
    }
}
