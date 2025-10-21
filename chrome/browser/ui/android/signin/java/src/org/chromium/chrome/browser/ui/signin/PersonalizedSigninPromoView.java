// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.ui.widget.ButtonCompat;

/** Container view for personalized signin promos. */
@NullMarked
public class PersonalizedSigninPromoView extends FrameLayout {
    private ImageView mImage;
    private ImageButton mDismissButton;
    private TextView mTitle;
    private TextView mDescription;
    private ButtonCompat mPrimaryButton;

    // TODO(crbug.com/448227402)
    // This is needed because this field is not supposed to be null, but there's no secondary button
    // in the seamless sign-in promo `compact`. It should be removed after the end of the experiment
    // when the final layout will be chosen.
    @SuppressWarnings("NullAway")
    private Button mSecondaryButton;

    // TODO(crbug.com/448227402)
    // This is needed because this field is not supposed to be null, but there's no account picker
    // in the seamless sign-in promo `twoButtons` and in the current promo. It should be removed
    // after the end of the experiment when the final layout will be chosen.
    @SuppressWarnings("NullAway")
    private View mSelectedAccountView;

    public PersonalizedSigninPromoView(Context context, AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context).inflate(getLayoutResource(), this, true);
    }

    private int getLayoutResource() {
        if (SigninFeatureMap.getInstance().getSeamlessSigninPromoType()
                == SigninFeatureMap.SeamlessSigninPromoType.TWO_BUTTONS) {
            return R.layout.two_buttons_signin_promo_view;
        }
        if (SigninFeatureMap.getInstance().getSeamlessSigninPromoType()
                == SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
            return R.layout.compact_signin_promo_view;
        }
        return R.layout.sync_promo_view;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        if (SigninFeatureMap.getInstance().getSeamlessSigninPromoType()
                == SigninFeatureMap.SeamlessSigninPromoType.NON_SEAMLESS) {
            mImage = findViewById(R.id.sync_promo_image);
            mDismissButton = findViewById(R.id.sync_promo_close_button);
            mPrimaryButton = findViewById(R.id.sync_promo_signin_button);
            mSecondaryButton = findViewById(R.id.sync_promo_choose_account_button);
            mTitle = findViewById(R.id.sync_promo_title);
            mDescription = findViewById(R.id.sync_promo_description);
        } else {
            mDismissButton = findViewById(R.id.signin_promo_dismiss_button);
            mTitle = findViewById(R.id.signin_promo_title);
            mDescription = findViewById(R.id.signin_promo_description);
            mPrimaryButton = findViewById(R.id.signin_promo_primary_button);
            if (SigninFeatureMap.getInstance().getSeamlessSigninPromoType()
                    == SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
                mImage = findViewById(R.id.account_image);
                mSelectedAccountView = findViewById(R.id.account_picker_selected_account);
            } else {
                mImage = findViewById(R.id.signin_promo_image);
                mSecondaryButton = findViewById(R.id.signin_promo_secondary_button);
            }
        }
    }

    /**
     * @return A reference to the image of the promo.
     */
    public ImageView getImage() {
        return mImage;
    }

    /**
     * @return A reference to the dismiss button.
     */
    public ImageButton getDismissButton() {
        return mDismissButton;
    }

    /**
     * @return A reference to the title of the sync promo.
     */
    public TextView getTitle() {
        return mTitle;
    }

    /**
     * @return A reference to the description of the promo.
     */
    public TextView getDescription() {
        return mDescription;
    }

    /**
     * @return A reference to the signin button.
     */
    public ButtonCompat getPrimaryButton() {
        return mPrimaryButton;
    }

    /**
     * @return A reference to the choose account button.
     */
    public Button getSecondaryButton() {
        return mSecondaryButton;
    }

    /**
     * @return A reference to the selected account view.
     */
    public View getSelectedAccountView() {
        return mSelectedAccountView;
    }

    /** Sets the card's background for R.id.signin_promo_view_wrapper. */
    public void setCardBackgroundResource(@DrawableRes int resId) {
        findViewById(R.id.signin_promo_view_wrapper).setBackgroundResource(resId);
    }
}
