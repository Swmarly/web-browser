// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createCreditCard;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createCreditCardSuggestion;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createVirtualCreditCard;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_AFFILIATED_LOYALTY_CARDS_SCREEN_INDEX_SELECTED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_ALL_LOYALTY_CARDS_SCREEN_INDEX_SELECTED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_CREDIT_CARD_INDEX_SELECTED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_IBAN_INDEX_SELECTED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_IBAN_OUTCOME_HISTOGRAM;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_LOYALTY_CARD_OUTCOME_HISTOGRAM;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_NUMBER_OF_AFFILIATED_LOYALTY_CARDS_SHOWN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_NUMBER_OF_IBANS_SHOWN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TOUCH_TO_FILL_NUMBER_OF_LOYALTY_CARDS_SHOWN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.APPLY_ISSUER_DEACTIVATED_STYLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_ICON_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_LINKED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_SELECTION_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ON_ISSUER_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerTosTextItemProperties.BNPL_TOS_ICON_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerTosTextItemProperties.DESCRIPTION_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressFooterProperties.APPLY_LINK_DEACTIVATED_STYLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressFooterProperties.HIDE_OPTIONS_LINK_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressFooterProperties.ON_LINK_CLICK_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressFooterProperties.TERMS_TEXT_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.BNPL_ICON_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.IS_ENABLED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.ON_BNPL_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.PRIMARY_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.SECONDARY_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ButtonProperties.ON_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ButtonProperties.TEXT_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.APPLY_DEACTIVATED_STYLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.FIRST_LINE_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MAIN_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MAIN_TEXT_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MINOR_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.ON_CREDIT_CARD_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.SECOND_LINE_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ErrorDescriptionProperties.ERROR_DESCRIPTION_STRING;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FOCUSED_VIEW_ID_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.OPEN_MANAGEMENT_UI_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.OPEN_MANAGEMENT_UI_TITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SCAN_CREDIT_CARD_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.SUBTITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.TITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.TITLE_STRING;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_NICKNAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_VALUE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.ON_IBAN_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.ALL_LOYALTY_CARDS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_ISSUER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_SELECTION_PROGRESS_FOOTER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_SELECTION_PROGRESS_HEADER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_TOS_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.ERROR_DESCRIPTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.FOOTER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.IBAN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.LOYALTY_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.TERMS_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.TOS_FOOTER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.WALLET_SETTINGS_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.LOYALTY_CARD_NUMBER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.MERCHANT_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.ON_LOYALTY_CARD_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_CLOSED_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_CONTENT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_FULL_HEIGHT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_HALF_HEIGHT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.ALL_LOYALTY_CARDS_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.BNPL_ISSUER_SELECTION_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.BNPL_ISSUER_TOS_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.ERROR_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.HOME_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.PROGRESS_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TermsLabelProperties.TERMS_LABEL_TEXT_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TosFooterProperties.LEGAL_MESSAGE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.VISIBLE;

import android.app.Activity;
import android.text.SpannableString;
import android.text.TextUtils;

import androidx.annotation.StringRes;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.ContextUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillImageFetcher;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillResourceProvider;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TouchToFillCreditCardOutcome;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TouchToFillIbanOutcome;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodMediator.TouchToFillLoyaltyCardOutcome;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.AllLoyaltyCardsItemProperties;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.LoyaltyCard;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.autofill.payments.BnplIssuerContext;
import org.chromium.components.autofill.payments.BnplIssuerTosDetail;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.components.payments.ui.test_support.FakeClock;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.TimeoutException;
import java.util.function.Consumer;
import java.util.stream.Collectors;
import java.util.stream.StreamSupport;

/**
 * Tests for {@link TouchToFillPaymentMethodCoordinator} and {@link
 * TouchToFillPaymentMethodMediator}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
@DisableFeatures({AutofillFeatures.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
public class TouchToFillPaymentMethodControllerRobolectricTest {
    private static final CreditCard VISA =
            createCreditCard(
                    "Visa",
                    "4111111111111111",
                    "5",
                    "2050",
                    true,
                    "Visa",
                    "• • • • 1111",
                    0,
                    "visa");
    private static final CreditCard NICKNAMED_VISA =
            createCreditCard(
                    "Visa",
                    "4111111111111111",
                    "5",
                    "2050",
                    true,
                    "Best Card",
                    "• • • • 1111",
                    0,
                    "visa");
    private static final CreditCard MASTERCARD =
            createCreditCard(
                    "MasterCard",
                    "5555555555554444",
                    "8",
                    "2050",
                    true,
                    "MasterCard",
                    "• • • • 4444",
                    0,
                    "mastercard");
    private static final CreditCard VIRTUAL_CARD =
            createVirtualCreditCard(
                    /* name= */ "Visa",
                    /* number= */ "4111111111111111",
                    /* month= */ "5",
                    /* year= */ "2050",
                    /* network= */ "Visa",
                    /* iconId= */ 0,
                    /* cardNameForAutofillDisplay= */ "Visa",
                    /* obfuscatedLastFourDigits= */ "• • • • 1111");

    private static final Iban LOCAL_IBAN =
            Iban.createLocal(
                    /* guid= */ "000000111111",
                    /* label= */ "CH56 **** **** **** *800 9",
                    /* nickname= */ "My brother's IBAN",
                    /* value= */ "CH5604835012345678009");

    private static final Iban LOCAL_IBAN_NO_NICKNAME =
            Iban.createLocal(
                    /* guid= */ "000000222222",
                    /* label= */ "FR76 **** **** **** **** ***0 189",
                    /* nickname= */ "",
                    /* value= */ "FR7630006000011234567890189");

    private static final LoyaltyCard LOYALTY_CARD_1 =
            new LoyaltyCard(
                    /* loyaltyCardId= */ "cvs",
                    /* merchantName= */ "CVS Pharmacy",
                    /* programName= */ "Loyalty program",
                    /* programLogo= */ new GURL("https://site.com/icon.png"),
                    /* loyaltyCardNumber= */ "1234",
                    /* merchantDomains= */ Collections.emptyList());

    private static final LoyaltyCard LOYALTY_CARD_2 =
            new LoyaltyCard(
                    /* loyaltyCardId= */ "stb",
                    /* merchantName= */ "Starbucks",
                    /* programName= */ "Coffee pro",
                    /* programLogo= */ new GURL("https://coffee.com/logo.png"),
                    /* loyaltyCardNumber= */ "4321",
                    /* merchantDomains= */ Collections.emptyList());

    private static final AutofillSuggestion VISA_SUGGESTION =
            createCreditCardSuggestion(
                    VISA.getCardNameForAutofillDisplay(),
                    VISA.getObfuscatedLastFourDigits(),
                    VISA.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL(""),
                    VISA.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    VISA.getGUID(),
                    VISA.getIsLocal());
    private static final AutofillSuggestion VISA_SUGGESTION_WITH_CARD_BENEFITS =
            createCreditCardSuggestion(
                    VISA.getCardNameForAutofillDisplay(),
                    VISA.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "2% cashback on travel",
                    VISA.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    VISA.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ true,
                    VISA.getGUID(),
                    VISA.getIsLocal());
    private static final AutofillSuggestion NICKNAMED_VISA_SUGGESTION =
            createCreditCardSuggestion(
                    NICKNAMED_VISA.getCardNameForAutofillDisplay(),
                    NICKNAMED_VISA.getObfuscatedLastFourDigits(),
                    NICKNAMED_VISA.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    String.format(
                            "%s %s",
                            NICKNAMED_VISA.getCardNameForAutofillDisplay(),
                            NICKNAMED_VISA.getBasicCardIssuerNetwork()),
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    NICKNAMED_VISA.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    NICKNAMED_VISA.getGUID(),
                    NICKNAMED_VISA.getIsLocal());
    private static final AutofillSuggestion MASTERCARD_SUGGESTION =
            createCreditCardSuggestion(
                    MASTERCARD.getCardNameForAutofillDisplay(),
                    MASTERCARD.getObfuscatedLastFourDigits(),
                    MASTERCARD.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    MASTERCARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    MASTERCARD.getGUID(),
                    MASTERCARD.getIsLocal());
    private static final AutofillSuggestion NON_ACCEPTABLE_VIRTUAL_CARD_SUGGESTION =
            createCreditCardSuggestion(
                    VIRTUAL_CARD.getCardNameForAutofillDisplay(),
                    VIRTUAL_CARD.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "Merchant doesn't accept this virtual card",
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.VIRTUAL_CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    VIRTUAL_CARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ true,
                    /* shouldDisplayTermsAvailable= */ false,
                    VIRTUAL_CARD.getGUID(),
                    VIRTUAL_CARD.getIsLocal());
    private static final AutofillSuggestion ACCEPTABLE_VIRTUAL_CARD_SUGGESTION =
            createCreditCardSuggestion(
                    VIRTUAL_CARD.getCardNameForAutofillDisplay(),
                    VIRTUAL_CARD.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "Virtual Card",
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.VIRTUAL_CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL(AutofillUiUtils.CAPITAL_ONE_ICON_URL),
                    VIRTUAL_CARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    VIRTUAL_CARD.getGUID(),
                    VIRTUAL_CARD.getIsLocal());
    private static final AutofillSuggestion VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS =
            createCreditCardSuggestion(
                    VIRTUAL_CARD.getCardNameForAutofillDisplay(),
                    VIRTUAL_CARD.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "2% cashback on travel",
                    /* secondarySubLabel= */ "Virtual card",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.VIRTUAL_CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    VIRTUAL_CARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ true,
                    VIRTUAL_CARD.getGUID(),
                    VIRTUAL_CARD.getIsLocal());
    private static final AutofillSuggestion BNPL_SUGGESTION =
            createCreditCardSuggestion(
                    /* label= */ "Pay later options",
                    /* secondaryLabel= */ "",
                    /* subLabel= */ "Available for purchases over $35",
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.BNPL_ENTRY,
                    /* customIconUrl= */ new GURL(""),
                    /* iconId= */ R.drawable.bnpl_icon_generic,
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    /* guid= */ "",
                    /* isLocalPaymentsMethod= */ false);
    private static final AutofillSuggestion DEACTIVATED_BNPL_SUGGESTION =
            createCreditCardSuggestion(
                    /* label= */ "Pay later options",
                    /* secondaryLabel= */ "",
                    /* subLabel= */ "Available for purchases over $35",
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.BNPL_ENTRY,
                    /* customIconUrl= */ new GURL(""),
                    /* iconId= */ R.drawable.bnpl_icon_generic,
                    /* applyDeactivatedStyle= */ true,
                    /* shouldDisplayTermsAvailable= */ false,
                    /* guid= */ "",
                    /* isLocalPaymentsMethod= */ false);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_AFFIRM_LINKED =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.affirm_linked,
                    /* issuerId= */ "affirm",
                    /* displayName= */ "Affirm",
                    /* selectionText= */ "Monthly or 4 installments",
                    /* isLinked= */ true,
                    /* isEligible= */ true);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_AFFIRM_UNLINKED =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.affirm_unlinked,
                    /* issuerId= */ "affirm",
                    /* displayName= */ "Affirm",
                    /* selectionText= */ "Monthly or 4 installments",
                    /* isLinked= */ false,
                    /* isEligible= */ true);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_KLARNA_LINKED =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.klarna_linked,
                    /* issuerId= */ "klarna",
                    /* displayName= */ "Klarna",
                    /* selectionText= */ "Pay in low monthly installments",
                    /* isLinked= */ true,
                    /* isEligible= */ true);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_KLARNA_UNLINKED =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.klarna_unlinked,
                    /* issuerId= */ "klarna",
                    /* displayName= */ "Klarna",
                    /* selectionText= */ "Pay in low monthly installments",
                    /* isLinked= */ false,
                    /* isEligible= */ true);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_ZIP_LINKED =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.zip_linked,
                    /* issuerId= */ "zip",
                    /* displayName= */ "Zip",
                    /* selectionText= */ "Pay in easy installments",
                    /* isLinked= */ true,
                    /* isEligible= */ true);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_ZIP_UNLINKED =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.zip_unlinked,
                    /* issuerId= */ "zip",
                    /* displayName= */ "Zip",
                    /* selectionText= */ "Pay in easy installments",
                    /* isLinked= */ false,
                    /* isEligible= */ true);
    private static final BnplIssuerContext
            BNPL_ISSUER_CONTEXT_INELIGIBLE_NOT_SUPPORTED_BY_MERCHANT =
                    new BnplIssuerContext(
                            /* iconId= */ R.drawable.affirm_linked,
                            /* issuerId= */ "affirm",
                            /* displayName= */ "Affirm",
                            /* selectionText= */ "Not supported by merchant",
                            /* isLinked= */ true,
                            /* isEligible= */ false);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_INELIGIBLE_CHECKOUT_AMOUNT_TOO_LOW =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.klarna_linked,
                    /* issuerId= */ "klarna",
                    /* displayName= */ "Klarna",
                    /* selectionText= */ "Purchase must be over $50.00",
                    /* isLinked= */ true,
                    /* isEligible= */ false);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_INELIGIBLE_CHECKOUT_AMOUNT_TOO_HIGH =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.zip_unlinked,
                    /* issuerId= */ "zip",
                    /* displayName= */ "Zip",
                    /* selectionText= */ "Purchase must be under $10,000.00",
                    /* isLinked= */ false,
                    /* isEligible= */ false);
    private static final String LEGAL_MESSAGE_LINE = "legal message line";
    private static final Consumer<String> MOCK_LINK_OPENER = mock(Consumer.class);
    private static final BnplIssuerTosDetail BNPL_ISSUER_TOS_DETAIL =
            new BnplIssuerTosDetail(
                    /* headerIconDrawableId= */ R.drawable.bnpl_icon_generic,
                    /* headerIconDarkDrawableId= */ R.drawable.bnpl_icon_generic,
                    /* title= */ "title for affirm",
                    /* reviewText= */ "Review text for affirm",
                    /* approveText= */ "Approve text for affirm",
                    /* linkText= */ new SpannableString("Link text for affirm"),
                    /* legalMessages= */ new BnplIssuerTosDetail.LegalMessages(
                            Arrays.asList(new LegalMessageLine(LEGAL_MESSAGE_LINE)),
                            MOCK_LINK_OPENER));
    private static final String BNPL_HIDE_OPTIONS_LINK_FOOTER_TEXT =
            "To hide pay later options, go to <link>payment settings</link>";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final FakeClock mClock = new FakeClock();
    private TouchToFillPaymentMethodCoordinator mCoordinator;
    private PropertyModel mTouchToFillPaymentMethodModel;
    private final Activity mActivity;

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TouchToFillPaymentMethodComponent.Delegate mDelegateMock;
    @Mock private BottomSheetFocusHelper mBottomSheetFocusHelper;
    @Mock private AutofillImageFetcher mImageFetcher;
    @Mock private TouchToFillResourceProvider mResourceProvider;

    public TouchToFillPaymentMethodControllerRobolectricTest() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
    }

    @Before
    public void setUp() {
        ServiceLoaderUtil.setInstanceForTesting(
                TouchToFillResourceProvider.class, mResourceProvider);
        when(mResourceProvider.getLoyaltyCardHeaderDrawableId())
                .thenReturn(R.drawable.ic_globe_24dp);
        when(mBottomSheetController.requestShowContent(any(BottomSheetContent.class), anyBoolean()))
                .thenReturn(true);
        mCoordinator = new TouchToFillPaymentMethodCoordinator();
        mCoordinator.initialize(
                mActivity,
                mImageFetcher,
                mBottomSheetController,
                mDelegateMock,
                mBottomSheetFocusHelper);
        mTouchToFillPaymentMethodModel = mCoordinator.getModelForTesting();
        mCoordinator
                .getMediatorForTesting()
                .setInputProtectorForTesting(new InputProtector(mClock));
    }

    @After
    public void tearDown() {
        BNPL_SUGGESTION.getPaymentsPayload().setExtractedAmount(null);
    }

    @Test
    public void testAddsTheBottomSheetHelperToObserveTheSheetForCreditCard() {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);

        verify(mBottomSheetFocusHelper, times(1)).registerForOneTimeUse();
    }

    @Test
    public void testCreatesValidDefaultCreditCardModel() {
        assertNotNull(mTouchToFillPaymentMethodModel.get(SHEET_ITEMS));
        assertNotNull(mTouchToFillPaymentMethodModel.get(DISMISS_HANDLER));
        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(HOME_SCREEN));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(false));
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);

        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_CONTENT_DESCRIPTION_ID),
                is(R.string.autofill_payment_method_bottom_sheet_content_description));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_HALF_HEIGHT_DESCRIPTION_ID),
                is(R.string.autofill_payment_method_bottom_sheet_half_height));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_FULL_HEIGHT_DESCRIPTION_ID),
                is(R.string.autofill_payment_method_bottom_sheet_full_height));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_CLOSED_DESCRIPTION_ID),
                is(R.string.autofill_payment_method_bottom_sheet_closed));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));
    }

    @Test
    public void testShowCreditCardSuggestionsWithOneEntry() throws TimeoutException {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, 1));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(1));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.fre_product_logo));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_payment_method_bottom_sheet_title));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, VISA_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertThat(cardSuggestionModel.get().get(MAIN_TEXT), is(VISA_SUGGESTION.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT), is(VISA_SUGGESTION.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL), is(VISA_SUGGESTION.getSublabel()));
        assertFalse(cardSuggestionModel.get().get(APPLY_DEACTIVATED_STYLE));
    }

    @Test
    public void testShowCreditCardSuggestionsWithTwoEntries() throws TimeoutException {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ false);

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, 2));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(2));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.fre_product_logo));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_payment_method_bottom_sheet_title));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, VISA_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertThat(cardSuggestionModel.get().get(MAIN_TEXT), is(VISA_SUGGESTION.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT), is(VISA_SUGGESTION.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL), is(VISA_SUGGESTION.getSublabel()));

        cardSuggestionModel = getCardSuggestionModel(itemList, MASTERCARD_SUGGESTION);
        assertThat(cardSuggestionModel.get().get(MAIN_TEXT), is(MASTERCARD_SUGGESTION.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT),
                is(MASTERCARD_SUGGESTION.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL),
                is(MASTERCARD_SUGGESTION.getSublabel()));
    }

    @Test
    public void testShowCreditCardSuggestionsWithNonAcceptableEntries() throws TimeoutException {
        HistogramWatcher metricsWatcher =
                HistogramWatcher.newSingleRecordWatcher(TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, 2);
        mCoordinator.showPaymentMethods(
                List.of(NON_ACCEPTABLE_VIRTUAL_CARD_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ false);

        metricsWatcher.assertExpected();

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(2));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.google_pay));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_payment_method_bottom_sheet_title));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, NON_ACCEPTABLE_VIRTUAL_CARD_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertTrue(cardSuggestionModel.get().get(APPLY_DEACTIVATED_STYLE));

        cardSuggestionModel = getCardSuggestionModel(itemList, MASTERCARD_SUGGESTION);
        assertFalse(cardSuggestionModel.get().get(APPLY_DEACTIVATED_STYLE));
    }

    @Test
    public void testShowCreditCardSuggestionsWithCardBenefits() throws TimeoutException {
        mCoordinator.showPaymentMethods(
                List.of(
                        MASTERCARD_SUGGESTION,
                        VISA_SUGGESTION_WITH_CARD_BENEFITS,
                        VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS),
                /* shouldShowScanCreditCard= */ false);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(3));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.google_pay));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_payment_method_bottom_sheet_title));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, MASTERCARD_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertThat(cardSuggestionModel.get().get(MAIN_TEXT), is(MASTERCARD_SUGGESTION.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT),
                is(MASTERCARD_SUGGESTION.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL),
                is(MASTERCARD_SUGGESTION.getSublabel()));
        // If card benefits are not present, the second line in labels has empty value.
        assertTrue(cardSuggestionModel.get().get(SECOND_LINE_LABEL).isEmpty());

        cardSuggestionModel = getCardSuggestionModel(itemList, VISA_SUGGESTION_WITH_CARD_BENEFITS);
        assertTrue(cardSuggestionModel.isPresent());
        assertThat(
                cardSuggestionModel.get().get(MAIN_TEXT),
                is(VISA_SUGGESTION_WITH_CARD_BENEFITS.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT),
                is(VISA_SUGGESTION_WITH_CARD_BENEFITS.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL),
                is(VISA_SUGGESTION_WITH_CARD_BENEFITS.getSublabel()));
        assertThat(
                cardSuggestionModel.get().get(SECOND_LINE_LABEL),
                is(VISA_SUGGESTION_WITH_CARD_BENEFITS.getSecondarySublabel()));

        cardSuggestionModel =
                getCardSuggestionModel(itemList, VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS);
        assertThat(
                cardSuggestionModel.get().get(MAIN_TEXT),
                is(VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT),
                is(VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL),
                is(VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS.getSublabel()));
        assertThat(
                cardSuggestionModel.get().get(SECOND_LINE_LABEL),
                is(VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS.getSecondarySublabel()));
    }

    @Test
    public void testShowCreditCardSuggestionsWithBnplSuggestion() throws TimeoutException {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, BNPL_SUGGESTION), /* shouldShowScanCreditCard= */ false);

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, 2));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(1));
        assertThat(getModelsOfType(itemList, BNPL).size(), is(1));
        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        assertThat(getModelsOfType(itemList, FOOTER).size(), is(1));
        assertThat(getModelsOfType(itemList, FILL_BUTTON).size(), is(0));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, VISA_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertThat(cardSuggestionModel.get().get(MAIN_TEXT), is(VISA_SUGGESTION.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT), is(VISA_SUGGESTION.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL), is(VISA_SUGGESTION.getSublabel()));
        assertFalse(cardSuggestionModel.get().get(APPLY_DEACTIVATED_STYLE));

        Optional<PropertyModel> bnplSuggestionModel =
                getBnplSuggestionModel(itemList, BNPL_SUGGESTION);
        assertTrue(bnplSuggestionModel.isPresent());
        assertThat(bnplSuggestionModel.get().get(PRIMARY_TEXT), is(BNPL_SUGGESTION.getLabel()));
        assertThat(
                bnplSuggestionModel.get().get(SECONDARY_TEXT), is(BNPL_SUGGESTION.getSublabel()));
        assertThat(bnplSuggestionModel.get().get(BNPL_ICON_ID), is(BNPL_SUGGESTION.getIconId()));
        assertNotNull(bnplSuggestionModel.get().get(ON_BNPL_CLICK_ACTION));
        assertTrue(bnplSuggestionModel.get().get(IS_ENABLED));
    }

    @Test
    public void testShowCreditCardSuggestionsWithDeactivatedBnplSuggestion()
            throws TimeoutException {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, DEACTIVATED_BNPL_SUGGESTION),
                /* shouldShowScanCreditCard= */ false);

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_CARDS_SHOWN, 2));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, CREDIT_CARD).size(), is(1));
        assertThat(getModelsOfType(itemList, BNPL).size(), is(1));
        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        assertThat(getModelsOfType(itemList, FOOTER).size(), is(1));
        assertThat(getModelsOfType(itemList, FILL_BUTTON).size(), is(0));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, VISA_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertThat(cardSuggestionModel.get().get(MAIN_TEXT), is(VISA_SUGGESTION.getLabel()));
        assertThat(
                cardSuggestionModel.get().get(MINOR_TEXT), is(VISA_SUGGESTION.getSecondaryLabel()));
        assertThat(
                cardSuggestionModel.get().get(FIRST_LINE_LABEL), is(VISA_SUGGESTION.getSublabel()));
        assertFalse(cardSuggestionModel.get().get(APPLY_DEACTIVATED_STYLE));

        Optional<PropertyModel> bnplSuggestionModel =
                getBnplSuggestionModel(itemList, DEACTIVATED_BNPL_SUGGESTION);
        assertTrue(bnplSuggestionModel.isPresent());
        assertThat(
                bnplSuggestionModel.get().get(PRIMARY_TEXT),
                is(DEACTIVATED_BNPL_SUGGESTION.getLabel()));
        assertThat(
                bnplSuggestionModel.get().get(SECONDARY_TEXT),
                is(DEACTIVATED_BNPL_SUGGESTION.getSublabel()));
        assertThat(
                bnplSuggestionModel.get().get(BNPL_ICON_ID),
                is(DEACTIVATED_BNPL_SUGGESTION.getIconId()));
        assertNotNull(bnplSuggestionModel.get().get(ON_BNPL_CLICK_ACTION));
        assertFalse(bnplSuggestionModel.get().get(IS_ENABLED));
    }

    @Test
    public void testCallsDelegateWhenBnplSuggestionIsSelected() throws TimeoutException {
        Long extractedAmount = 100L;
        BNPL_SUGGESTION.getPaymentsPayload().setExtractedAmount(extractedAmount);
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, BNPL_SUGGESTION), /* shouldShowScanCreditCard= */ false);
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, BNPL).size(), is(1));
        Optional<PropertyModel> bnplSuggestionModel =
                getBnplSuggestionModel(itemList, BNPL_SUGGESTION);
        assertTrue(bnplSuggestionModel.isPresent());
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);

        bnplSuggestionModel.get().get(ON_BNPL_CLICK_ACTION).run();

        verify(mDelegateMock).bnplSuggestionSelected(extractedAmount);
    }

    @Test
    public void testShowProgressScreenForBnpl() {
        mCoordinator.getMediatorForTesting().showProgressScreen();

        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(PROGRESS_SCREEN));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_CONTENT_DESCRIPTION_ID),
                is(R.string.autofill_bnpl_progress_sheet_content_description));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_HALF_HEIGHT_DESCRIPTION_ID),
                is(R.string.autofill_bnpl_progress_sheet_half_height));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_FULL_HEIGHT_DESCRIPTION_ID),
                is(R.string.autofill_bnpl_progress_sheet_full_height));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_CLOSED_DESCRIPTION_ID),
                is(R.string.autofill_bnpl_progress_sheet_closed));

        ModelList sheetItems = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(sheetItems.size(), is(2));

        ListItem bnplSelectionProgressHeaderItem = sheetItems.get(0);
        assertThat(
                bnplSelectionProgressHeaderItem.type,
                is(TouchToFillPaymentMethodProperties.ItemType.BNPL_SELECTION_PROGRESS_HEADER));
        assertFalse(
                bnplSelectionProgressHeaderItem.model.get(
                        TouchToFillPaymentMethodProperties.BnplSelectionProgressHeaderProperties
                                .BNPL_BACK_BUTTON_ENABLED));

        ListItem progressIconItem = sheetItems.get(1);
        assertThat(
                progressIconItem.type,
                is(TouchToFillPaymentMethodProperties.ItemType.PROGRESS_ICON));
        assertThat(
                progressIconItem.model.get(
                        TouchToFillPaymentMethodProperties.ProgressIconProperties
                                .PROGRESS_CONTENT_DESCRIPTION_ID),
                is(R.string.autofill_pending_dialog_loading_accessibility_description));
    }

    @Test
    public void testBnplSelectionProgressHeaderBackButtonReshowsHomeScreen() {
        // Show the credit card list first to populate the mediator's suggestions.
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ false);
        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(HOME_SCREEN));
        assertThat(
                getModelsOfType(mTouchToFillPaymentMethodModel.get(SHEET_ITEMS), CREDIT_CARD)
                        .size(),
                is(2));

        // Simulate switching to the BNPL progress screen.
        mCoordinator.getMediatorForTesting().showProgressScreen();
        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(PROGRESS_SCREEN));

        // Find the back button action in the BNPL header and invoke it.
        ModelList sheetItems = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(sheetItems.get(0).type, is(BNPL_SELECTION_PROGRESS_HEADER));
        PropertyModel bnplSelectionProgressHeaderModel = sheetItems.get(0).model;
        bnplSelectionProgressHeaderModel
                .get(
                        TouchToFillPaymentMethodProperties.BnplSelectionProgressHeaderProperties
                                .BNPL_ON_BACK_BUTTON_CLICKED)
                .run();

        // Verify that the home screen is shown again with the original credit card suggestions.
        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(HOME_SCREEN));
        sheetItems = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(sheetItems, CREDIT_CARD).size(), is(2));
        assertTrue(getCardSuggestionModel(sheetItems, VISA_SUGGESTION).isPresent());
        assertTrue(getCardSuggestionModel(sheetItems, MASTERCARD_SUGGESTION).isPresent());
    }

    @Test
    public void testShowBnplIssuerSelectionScreenWithLinkedIssuers() {
        mCoordinator
                .getMediatorForTesting()
                .showBnplIssuers(
                        List.of(
                                BNPL_ISSUER_CONTEXT_AFFIRM_LINKED,
                                BNPL_ISSUER_CONTEXT_KLARNA_LINKED,
                                BNPL_ISSUER_CONTEXT_ZIP_LINKED),
                        BNPL_HIDE_OPTIONS_LINK_FOOTER_TEXT);

        assertThat(
                mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN),
                is(BNPL_ISSUER_SELECTION_SCREEN));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        assertBnplIssuerSelectionScreenHasCorrectAccessibilityStrings(
                mTouchToFillPaymentMethodModel);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, BNPL_ISSUER).size(), is(3));

        assertBnplIssuerContextModelMatches(itemList, BNPL_ISSUER_CONTEXT_AFFIRM_LINKED);
        assertBnplIssuerContextModelMatches(itemList, BNPL_ISSUER_CONTEXT_KLARNA_LINKED);
        assertBnplIssuerContextModelMatches(itemList, BNPL_ISSUER_CONTEXT_ZIP_LINKED);

        assertFooterModelHasExpectedValues(
                itemList,
                /* expectedTermsTextId= */ R.string.autofill_bnpl_issuer_bottom_sheet_terms_label,
                /* expectedHideOptionsLinkText= */ BNPL_HIDE_OPTIONS_LINK_FOOTER_TEXT);
    }

    @Test
    public void testShowBnplIssuerSelectionScreenWithUnlinkedIssuers() {
        mCoordinator
                .getMediatorForTesting()
                .showBnplIssuers(
                        List.of(
                                BNPL_ISSUER_CONTEXT_AFFIRM_UNLINKED,
                                BNPL_ISSUER_CONTEXT_KLARNA_UNLINKED,
                                BNPL_ISSUER_CONTEXT_ZIP_UNLINKED),
                        BNPL_HIDE_OPTIONS_LINK_FOOTER_TEXT);

        assertThat(
                mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN),
                is(BNPL_ISSUER_SELECTION_SCREEN));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        assertBnplIssuerSelectionScreenHasCorrectAccessibilityStrings(
                mTouchToFillPaymentMethodModel);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, BNPL_ISSUER).size(), is(3));

        assertBnplIssuerContextModelMatches(itemList, BNPL_ISSUER_CONTEXT_AFFIRM_UNLINKED);
        assertBnplIssuerContextModelMatches(itemList, BNPL_ISSUER_CONTEXT_KLARNA_UNLINKED);
        assertBnplIssuerContextModelMatches(itemList, BNPL_ISSUER_CONTEXT_ZIP_UNLINKED);

        assertFooterModelHasExpectedValues(
                itemList,
                /* expectedTermsTextId= */ R.string.autofill_bnpl_issuer_bottom_sheet_terms_label,
                /* expectedHideOptionsLinkText= */ BNPL_HIDE_OPTIONS_LINK_FOOTER_TEXT);
    }

    @Test
    public void testShowBnplIssuerSelectionScreenWithIneligibleIssuers() {
        mCoordinator
                .getMediatorForTesting()
                .showBnplIssuers(
                        List.of(
                                BNPL_ISSUER_CONTEXT_INELIGIBLE_NOT_SUPPORTED_BY_MERCHANT,
                                BNPL_ISSUER_CONTEXT_INELIGIBLE_CHECKOUT_AMOUNT_TOO_LOW,
                                BNPL_ISSUER_CONTEXT_INELIGIBLE_CHECKOUT_AMOUNT_TOO_HIGH),
                        BNPL_HIDE_OPTIONS_LINK_FOOTER_TEXT);

        assertThat(
                mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN),
                is(BNPL_ISSUER_SELECTION_SCREEN));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        assertBnplIssuerSelectionScreenHasCorrectAccessibilityStrings(
                mTouchToFillPaymentMethodModel);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, BNPL_ISSUER).size(), is(3));

        assertBnplIssuerContextModelMatches(
                itemList, BNPL_ISSUER_CONTEXT_INELIGIBLE_NOT_SUPPORTED_BY_MERCHANT);
        assertBnplIssuerContextModelMatches(
                itemList, BNPL_ISSUER_CONTEXT_INELIGIBLE_CHECKOUT_AMOUNT_TOO_LOW);
        assertBnplIssuerContextModelMatches(
                itemList, BNPL_ISSUER_CONTEXT_INELIGIBLE_CHECKOUT_AMOUNT_TOO_HIGH);

        assertFooterModelHasExpectedValues(
                itemList,
                /* expectedTermsTextId= */ R.string.autofill_bnpl_issuer_bottom_sheet_terms_label,
                /* expectedHideOptionsLinkText= */ BNPL_HIDE_OPTIONS_LINK_FOOTER_TEXT);
    }

    @Test
    public void testShowBnplIssuerScreenFooterLinkOpensPaymentMethodSettings() {
        mCoordinator
                .getMediatorForTesting()
                .showBnplIssuers(
                        List.of(BNPL_ISSUER_CONTEXT_AFFIRM_LINKED),
                        BNPL_HIDE_OPTIONS_LINK_FOOTER_TEXT);
        assertThat(
                mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN),
                is(BNPL_ISSUER_SELECTION_SCREEN));

        ModelList sheetItems = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        Optional<PropertyModel> footerModel = getBnplSelectionProgressFooterModel(sheetItems);
        assertTrue(footerModel.isPresent());

        footerModel.get().get(ON_LINK_CLICK_CALLBACK).onResult(null);

        verify(mDelegateMock).showPaymentMethodSettings();
    }

    @Test
    public void testUpdateBnplPaymentMethodWithUnSupportedAmount() throws TimeoutException {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, BNPL_SUGGESTION), /* shouldShowScanCreditCard= */ false);
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        Optional<PropertyModel> bnplSuggestionModel =
                getBnplSuggestionModel(itemList, BNPL_SUGGESTION);
        assertTrue(bnplSuggestionModel.isPresent());
        assertThat(bnplSuggestionModel.get().get(PRIMARY_TEXT), is(BNPL_SUGGESTION.getLabel()));
        assertThat(
                bnplSuggestionModel.get().get(SECONDARY_TEXT), is(BNPL_SUGGESTION.getSublabel()));
        assertThat(bnplSuggestionModel.get().get(BNPL_ICON_ID), is(BNPL_SUGGESTION.getIconId()));
        assertTrue(bnplSuggestionModel.get().get(IS_ENABLED));
        assertNull(BNPL_SUGGESTION.getPaymentsPayload().getExtractedAmount());

        mCoordinator.updateBnplPaymentMethod(
                /* extractedAmount= */ 5L, /* isAmountSupportedByAnyIssuer= */ false);

        assertThat(bnplSuggestionModel.get().get(PRIMARY_TEXT), is(BNPL_SUGGESTION.getLabel()));
        String expectedSecondaryText =
                ContextUtils.getApplicationContext()
                        .getString(
                                R.string.autofill_bnpl_suggestion_label_for_unavailable_purchase);
        assertThat(bnplSuggestionModel.get().get(SECONDARY_TEXT), is(expectedSecondaryText));
        assertThat(bnplSuggestionModel.get().get(BNPL_ICON_ID), is(BNPL_SUGGESTION.getIconId()));
        assertFalse(bnplSuggestionModel.get().get(IS_ENABLED));
        assertNull(BNPL_SUGGESTION.getPaymentsPayload().getExtractedAmount());
    }

    @Test
    public void testUpdateBnplPaymentMethodWithInvalidAmount() throws TimeoutException {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, BNPL_SUGGESTION), /* shouldShowScanCreditCard= */ false);
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        Optional<PropertyModel> bnplSuggestionModel =
                getBnplSuggestionModel(itemList, BNPL_SUGGESTION);
        assertTrue(bnplSuggestionModel.isPresent());
        assertThat(bnplSuggestionModel.get().get(PRIMARY_TEXT), is(BNPL_SUGGESTION.getLabel()));
        assertThat(
                bnplSuggestionModel.get().get(SECONDARY_TEXT), is(BNPL_SUGGESTION.getSublabel()));
        assertThat(bnplSuggestionModel.get().get(BNPL_ICON_ID), is(BNPL_SUGGESTION.getIconId()));
        assertTrue(bnplSuggestionModel.get().get(IS_ENABLED));
        assertNull(BNPL_SUGGESTION.getPaymentsPayload().getExtractedAmount());

        mCoordinator.updateBnplPaymentMethod(
                /* extractedAmount= */ null, /* isAmountSupportedByAnyIssuer= */ false);

        assertThat(bnplSuggestionModel.get().get(PRIMARY_TEXT), is(BNPL_SUGGESTION.getLabel()));
        String expectedSecondaryText =
                ContextUtils.getApplicationContext()
                        .getString(
                                R.string.autofill_bnpl_suggestion_label_for_unavailable_purchase);
        assertThat(bnplSuggestionModel.get().get(SECONDARY_TEXT), is(expectedSecondaryText));
        assertThat(bnplSuggestionModel.get().get(BNPL_ICON_ID), is(BNPL_SUGGESTION.getIconId()));
        assertFalse(bnplSuggestionModel.get().get(IS_ENABLED));
        assertNull(BNPL_SUGGESTION.getPaymentsPayload().getExtractedAmount());
    }

    @Test
    public void testUpdateBnplPaymentMethodWithValidAmount() throws TimeoutException {
        long extractedAmount = 100L;
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, BNPL_SUGGESTION), /* shouldShowScanCreditCard= */ false);
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        Optional<PropertyModel> bnplSuggestionModel =
                getBnplSuggestionModel(itemList, BNPL_SUGGESTION);
        assertTrue(bnplSuggestionModel.isPresent());
        assertThat(bnplSuggestionModel.get().get(PRIMARY_TEXT), is(BNPL_SUGGESTION.getLabel()));
        assertThat(
                bnplSuggestionModel.get().get(SECONDARY_TEXT), is(BNPL_SUGGESTION.getSublabel()));
        assertThat(bnplSuggestionModel.get().get(BNPL_ICON_ID), is(BNPL_SUGGESTION.getIconId()));
        assertTrue(bnplSuggestionModel.get().get(IS_ENABLED));
        assertNull(BNPL_SUGGESTION.getPaymentsPayload().getExtractedAmount());

        mCoordinator.updateBnplPaymentMethod(
                extractedAmount, /* isAmountSupportedByAnyIssuer= */ true);

        assertThat(bnplSuggestionModel.get().get(PRIMARY_TEXT), is(BNPL_SUGGESTION.getLabel()));
        assertThat(
                bnplSuggestionModel.get().get(SECONDARY_TEXT), is(BNPL_SUGGESTION.getSublabel()));
        assertThat(bnplSuggestionModel.get().get(BNPL_ICON_ID), is(BNPL_SUGGESTION.getIconId()));
        assertTrue(bnplSuggestionModel.get().get(IS_ENABLED));
        assertThat(BNPL_SUGGESTION.getPaymentsPayload().getExtractedAmount(), is(extractedAmount));
    }

    @Test
    public void testShowBnplIssuerTos() throws TimeoutException {
        mCoordinator.showBnplIssuerTos(BNPL_ISSUER_TOS_DETAIL);

        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(BNPL_ISSUER_TOS_SCREEN));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);

        List<PropertyModel> headerModel = getModelsOfType(itemList, HEADER);
        assertThat(headerModel.size(), is(1));
        assertThat(headerModel.get(0).get(TITLE_STRING), is(BNPL_ISSUER_TOS_DETAIL.getTitle()));
        assertThat(headerModel.get(0).get(IMAGE_DRAWABLE_ID), is(R.drawable.bnpl_icon_generic));

        List<PropertyModel> bnplTosItemModel = getModelsOfType(itemList, BNPL_TOS_TEXT);
        assertThat(bnplTosItemModel.size(), is(3));
        assertThat(
                bnplTosItemModel.get(0).get(DESCRIPTION_TEXT),
                is(BNPL_ISSUER_TOS_DETAIL.getReviewText()));
        assertThat(bnplTosItemModel.get(0).get(BNPL_TOS_ICON_ID), is(R.drawable.checklist));
        assertThat(
                bnplTosItemModel.get(1).get(DESCRIPTION_TEXT),
                is(BNPL_ISSUER_TOS_DETAIL.getApproveText()));
        assertThat(bnplTosItemModel.get(1).get(BNPL_TOS_ICON_ID), is(R.drawable.receipt_long));
        assertThat(
                bnplTosItemModel.get(2).get(DESCRIPTION_TEXT).toString(),
                is(BNPL_ISSUER_TOS_DETAIL.getLinkText().toString()));
        assertThat(bnplTosItemModel.get(2).get(BNPL_TOS_ICON_ID), is(R.drawable.add_link));

        List<PropertyModel> footerModel = getModelsOfType(itemList, TOS_FOOTER);
        assertThat(footerModel.size(), is(1));
        BnplIssuerTosDetail.LegalMessages legalMessages = footerModel.get(0).get(LEGAL_MESSAGE);
        assertThat(legalMessages.mLines.size(), is(1));
        assertThat(legalMessages.mLines.get(0).text, is(LEGAL_MESSAGE_LINE));
        assertThat(legalMessages.mLinkOpener, is(MOCK_LINK_OPENER));
    }

    @Test
    public void testShowErrorScreen() {
        final String title = "Something went wrong";
        final String description =
                "Pay later is unavailable at this time. Try again or choose another payment"
                        + " method.";
        mCoordinator.getMediatorForTesting().showErrorScreen(title, description);

        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(ERROR_SCREEN));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_CONTENT_DESCRIPTION_ID),
                is(R.string.autofill_bnpl_error_sheet_content_description));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_HALF_HEIGHT_DESCRIPTION_ID),
                is(R.string.autofill_bnpl_error_sheet_half_height));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_FULL_HEIGHT_DESCRIPTION_ID),
                is(R.string.autofill_bnpl_error_sheet_full_height));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_CLOSED_DESCRIPTION_ID),
                is(R.string.autofill_bnpl_error_sheet_closed));

        ModelList sheetItems = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(sheetItems.size(), is(3));

        ListItem headerItem = sheetItems.get(0);
        assertThat(headerItem.type, is(HEADER));
        assertThat(headerItem.model.get(IMAGE_DRAWABLE_ID), is(R.drawable.error_icon));
        assertThat(headerItem.model.get(TITLE_STRING), is(title));

        ListItem descriptionItem = sheetItems.get(1);
        assertThat(descriptionItem.type, is(ERROR_DESCRIPTION));
        assertThat(descriptionItem.model.get(ERROR_DESCRIPTION_STRING), is(description));

        ListItem buttonItem = sheetItems.get(2);
        assertThat(buttonItem.type, is(FILL_BUTTON));
        assertThat(buttonItem.model.get(TEXT_ID), is(R.string.autofill_bnpl_error_ok_button));
        assertNotNull(buttonItem.model.get(ON_CLICK_ACTION));
    }

    @Test
    public void testErrorScreenOkButtonCallsDelegate() {
        mCoordinator.getMediatorForTesting().showErrorScreen("Title", "Desc");

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        getModelsOfType(itemList, FILL_BUTTON).get(0).get(ON_CLICK_ACTION).run();
        verify(mDelegateMock).onErrorOkPressed();
    }

    @Test
    public void testBenefitsTermsLabel_ShownWhenCardHasBenefits() {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION_WITH_CARD_BENEFITS), /* shouldShowScanCreditCard= */ true);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);

        assertTermsLabelModelHasExpectedTextId(
                itemList, R.string.autofill_payment_method_bottom_sheet_benefits_terms_label);
    }

    @Test
    public void testBenefitsTermsLabel_HiddenWhenCardHasNoBenefits() {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ true);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(0, getModelsOfType(itemList, TERMS_LABEL).size());
    }

    @Test
    public void testScanNewCardIsShownForCreditCards() {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);
        int lastItemPos = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS).size() - 1;
        mTouchToFillPaymentMethodModel
                .get(SHEET_ITEMS)
                .get(lastItemPos)
                .model
                .get(SCAN_CREDIT_CARD_CALLBACK)
                .run();
        verify(mDelegateMock).scanCreditCard();
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.SCAN_NEW_CARD));
    }

    @Test
    public void testShowPaymentMethodSettingsForCreditCards() {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);
        int lastItemPos = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS).size() - 1;
        mTouchToFillPaymentMethodModel
                .get(SHEET_ITEMS)
                .get(lastItemPos)
                .model
                .get(OPEN_MANAGEMENT_UI_CALLBACK)
                .run();
        verify(mDelegateMock).showPaymentMethodSettings();
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.MANAGE_PAYMENTS));
    }

    @Test
    public void testNoCallbackForCreditCardSuggestionOnSelectingItemBeforeInputTime() {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(
                        mTouchToFillPaymentMethodModel.get(SHEET_ITEMS), VISA_SUGGESTION);
        assertNotNull(cardSuggestionModel.get().get(ON_CREDIT_CARD_CLICK_ACTION));

        // Clicking after an interval less than the threshold should be a no-op.
        mClock.advanceCurrentTimeMillis(
                InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD - 100);
        cardSuggestionModel.get().get(ON_CREDIT_CARD_CLICK_ACTION).run();
        verify(mDelegateMock, times(0))
                .creditCardSuggestionSelected(VISA.getGUID(), VISA.getIsVirtual());

        // Clicking after the threshold should work.
        mClock.advanceCurrentTimeMillis(100);
        cardSuggestionModel.get().get(ON_CREDIT_CARD_CLICK_ACTION).run();
        verify(mDelegateMock, times(1))
                .creditCardSuggestionSelected(VISA.getGUID(), VISA.getIsVirtual());
    }

    @Test
    public void testCallsCallbackForCreditCardSuggestionOnSelectingItem() {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(
                        mTouchToFillPaymentMethodModel.get(SHEET_ITEMS), VISA_SUGGESTION);
        assertNotNull(cardSuggestionModel.get().get(ON_CREDIT_CARD_CLICK_ACTION));

        advanceClockAndClick(cardSuggestionModel.get());
        verify(mDelegateMock).creditCardSuggestionSelected(VISA.getGUID(), VISA.getIsVirtual());
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.CREDIT_CARD));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_INDEX_SELECTED, 0));
    }

    @Test
    public void testCallsCallbackForVirtualCardSuggestionOnSelectingItem() {
        mCoordinator.showPaymentMethods(
                List.of(ACCEPTABLE_VIRTUAL_CARD_SUGGESTION), /* shouldShowScanCreditCard= */ false);
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(
                        mTouchToFillPaymentMethodModel.get(SHEET_ITEMS),
                        ACCEPTABLE_VIRTUAL_CARD_SUGGESTION);
        assertNotNull(cardSuggestionModel.get().get(ON_CREDIT_CARD_CLICK_ACTION));

        advanceClockAndClick(cardSuggestionModel.get());
        verify(mDelegateMock)
                .creditCardSuggestionSelected(VIRTUAL_CARD.getGUID(), VIRTUAL_CARD.getIsVirtual());
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.VIRTUAL_CARD));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_INDEX_SELECTED, 0));
    }

    @Test
    public void testShowsContinueButtonWhenOneCreditCard() {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ true);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(1, getModelsOfType(itemList, FILL_BUTTON).size());
    }

    @Test
    public void testNoContinueButtonWhenManyCreditCards() {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(0, getModelsOfType(itemList, FILL_BUTTON).size());
    }

    @Test
    public void testDismissWithSwipeForCreditCard() {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);

        mTouchToFillPaymentMethodModel.get(DISMISS_HANDLER).onResult(StateChangeReason.SWIPE);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.DISMISS));
    }

    @Test
    public void testDismissWithTapForCreditCard() {
        HistogramWatcher metricsWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TOUCH_TO_FILL_CREDIT_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillCreditCardOutcome.DISMISS);
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);

        mTouchToFillPaymentMethodModel.get(DISMISS_HANDLER).onResult(StateChangeReason.TAP_SCRIM);

        metricsWatcher.assertExpected();
    }

    @Test
    public void testScanNewCardClick() {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ true);
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        getModelsOfType(itemList, FOOTER).get(0).get(SCAN_CREDIT_CARD_CALLBACK).run();

        verify(mDelegateMock).scanCreditCard();
    }

    @Test
    public void testManagePaymentMethodsClickForCreditCard() {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                /* shouldShowScanCreditCard= */ false);
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, FOOTER).size(), is(1));
        assertThat(
                getModelsOfType(itemList, FOOTER).get(0).get(OPEN_MANAGEMENT_UI_TITLE_ID),
                is(R.string.autofill_bottom_sheet_manage_payment_methods));
        getModelsOfType(itemList, FOOTER).get(0).get(OPEN_MANAGEMENT_UI_CALLBACK).run();

        verify(mDelegateMock).showPaymentMethodSettings();
    }

    @Test
    public void testContinueButtonClickForCreditCard() {
        mCoordinator.showPaymentMethods(
                List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        getModelsOfType(itemList, FILL_BUTTON).get(0).get(ON_CLICK_ACTION).run();
        verify(mDelegateMock).creditCardSuggestionSelected(VISA.getGUID(), VISA.getIsVirtual());
    }

    @Test
    public void testCardSuggestionModelForNicknamedCardContainsANetworkName() {
        mCoordinator.showPaymentMethods(
                List.of(NICKNAMED_VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);

        Optional<PropertyModel> cardSuggestionModel =
                getCardSuggestionModel(itemList, NICKNAMED_VISA_SUGGESTION);
        assertTrue(cardSuggestionModel.isPresent());
        assertEquals(
                "Best Card visa", cardSuggestionModel.get().get(MAIN_TEXT_CONTENT_DESCRIPTION));
    }

    @Test
    public void testCreatesValidDefaultIbanModel() {
        assertNotNull(mTouchToFillPaymentMethodModel.get(SHEET_ITEMS));
        assertNotNull(mTouchToFillPaymentMethodModel.get(DISMISS_HANDLER));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(false));

        mCoordinator.showIbans(List.of(LOCAL_IBAN));

        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_CONTENT_DESCRIPTION_ID),
                is(R.string.autofill_payment_method_bottom_sheet_content_description));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_HALF_HEIGHT_DESCRIPTION_ID),
                is(R.string.autofill_payment_method_bottom_sheet_half_height));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_FULL_HEIGHT_DESCRIPTION_ID),
                is(R.string.autofill_payment_method_bottom_sheet_full_height));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_CLOSED_DESCRIPTION_ID),
                is(R.string.autofill_payment_method_bottom_sheet_closed));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));
    }

    @Test
    public void testScanNewCardNotShownForIbans() {
        mCoordinator.showIbans(List.of(LOCAL_IBAN));
        int lastItemPos = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS).size() - 1;

        assertNull(
                mTouchToFillPaymentMethodModel
                        .get(SHEET_ITEMS)
                        .get(lastItemPos)
                        .model
                        .get(SCAN_CREDIT_CARD_CALLBACK));
    }

    @Test
    public void testShowIbansWithOneEntry() throws TimeoutException {
        mCoordinator.showIbans(List.of(LOCAL_IBAN));

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_IBANS_SHOWN, 1));
        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(HOME_SCREEN));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, IBAN).size(), is(1));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.fre_product_logo));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_payment_method_bottom_sheet_title));

        Optional<PropertyModel> ibanModel = getIbanModelByAutofillName(itemList, LOCAL_IBAN);
        assertTrue(ibanModel.isPresent());
        assertThat(ibanModel.get().get(IBAN_VALUE), is(LOCAL_IBAN.getLabel()));
        assertThat(ibanModel.get().get(IBAN_NICKNAME), is(LOCAL_IBAN.getNickname()));
    }

    @Test
    public void testShowIbansWithTwoEntries() throws TimeoutException {
        mCoordinator.showIbans(List.of(LOCAL_IBAN, LOCAL_IBAN_NO_NICKNAME));

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_NUMBER_OF_IBANS_SHOWN, 2));
        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(HOME_SCREEN));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, IBAN).size(), is(2));

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.fre_product_logo));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_payment_method_bottom_sheet_title));

        Optional<PropertyModel> ibanModel = getIbanModelByAutofillName(itemList, LOCAL_IBAN);
        assertTrue(ibanModel.isPresent());
        assertThat(ibanModel.get().get(IBAN_VALUE), is(LOCAL_IBAN.getLabel()));
        assertThat(ibanModel.get().get(IBAN_NICKNAME), is(LOCAL_IBAN.getNickname()));

        ibanModel = getIbanModelByAutofillName(itemList, LOCAL_IBAN_NO_NICKNAME);
        assertThat(ibanModel.get().get(IBAN_VALUE), is(LOCAL_IBAN_NO_NICKNAME.getLabel()));
        assertThat(ibanModel.get().get(IBAN_NICKNAME), is(LOCAL_IBAN_NO_NICKNAME.getNickname()));
    }

    @Test
    public void testShowPaymentMethodSettingsForIbans() {
        mCoordinator.showIbans(List.of(LOCAL_IBAN, LOCAL_IBAN_NO_NICKNAME));
        int lastItemPos = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS).size() - 1;
        mTouchToFillPaymentMethodModel
                .get(SHEET_ITEMS)
                .get(lastItemPos)
                .model
                .get(OPEN_MANAGEMENT_UI_CALLBACK)
                .run();
        verify(mDelegateMock).showPaymentMethodSettings();
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_IBAN_OUTCOME_HISTOGRAM,
                        TouchToFillIbanOutcome.MANAGE_PAYMENTS));
    }

    @Test
    public void testCallsDelegateForIbanOnSelectingItem() {
        mCoordinator.showIbans(List.of(LOCAL_IBAN));
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        Optional<PropertyModel> ibanModel =
                getIbanModelByAutofillName(
                        mTouchToFillPaymentMethodModel.get(SHEET_ITEMS), LOCAL_IBAN);
        assertTrue(ibanModel.isPresent());
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        ibanModel.get().get(ON_IBAN_CLICK_ACTION).run();
        verify(mDelegateMock).localIbanSuggestionSelected(LOCAL_IBAN.getGuid());
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_IBAN_OUTCOME_HISTOGRAM, TouchToFillIbanOutcome.IBAN));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        TOUCH_TO_FILL_IBAN_INDEX_SELECTED, 0));
    }

    @Test
    public void testShowsContinueButtonWhenOneIban() {
        mCoordinator.showIbans(List.of(LOCAL_IBAN));

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(1, getModelsOfType(itemList, FILL_BUTTON).size());
    }

    @Test
    public void testNoContinueButtonWhenManyIbans() {
        mCoordinator.showIbans(List.of(LOCAL_IBAN, LOCAL_IBAN_NO_NICKNAME));

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertEquals(0, getModelsOfType(itemList, FILL_BUTTON).size());
    }

    @Test
    public void testManagePaymentMethodsClickForIban() {
        mCoordinator.showIbans(List.of(LOCAL_IBAN));

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, FOOTER).size(), is(1));
        assertThat(
                getModelsOfType(itemList, FOOTER).get(0).get(OPEN_MANAGEMENT_UI_TITLE_ID),
                is(R.string.autofill_bottom_sheet_manage_payment_methods));
        getModelsOfType(itemList, FOOTER).get(0).get(OPEN_MANAGEMENT_UI_CALLBACK).run();

        verify(mDelegateMock).showPaymentMethodSettings();
    }

    @Test
    public void testShowOneLoyaltyCardFirstTime() throws TimeoutException {
        mCoordinator.showLoyaltyCards(
                List.of(LOYALTY_CARD_1), List.of(LOYALTY_CARD_1), /* firstTimeUsage= */ true);

        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_CONTENT_DESCRIPTION_ID),
                is(R.string.autofill_loyalty_card_bottom_sheet_content_description));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_HALF_HEIGHT_DESCRIPTION_ID),
                is(R.string.autofill_loyalty_card_bottom_sheet_half_height));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_FULL_HEIGHT_DESCRIPTION_ID),
                is(R.string.autofill_loyalty_card_bottom_sheet_full_height));
        assertThat(
                mTouchToFillPaymentMethodModel.get(SHEET_CLOSED_DESCRIPTION_ID),
                is(R.string.autofill_loyalty_card_bottom_sheet_closed));

        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(HOME_SCREEN));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.ic_globe_24dp));
        assertThat(
                headerModel.get(TITLE_ID),
                is(R.string.autofill_loyalty_card_first_time_usage_bottom_sheet_title));
        assertThat(
                headerModel.get(SUBTITLE_ID),
                is(R.string.autofill_loyalty_card_first_time_usage_bottom_sheet_subtitle));

        assertThat(getModelsOfType(itemList, LOYALTY_CARD).size(), is(1));
        PropertyModel loyaltyCardModel = itemList.get(1).model;
        assertThat(
                loyaltyCardModel.get(LOYALTY_CARD_NUMBER),
                is(LOYALTY_CARD_1.getLoyaltyCardNumber()));
        assertThat(loyaltyCardModel.get(MERCHANT_NAME), is(LOYALTY_CARD_1.getMerchantName()));

        assertThat(getModelsOfType(itemList, ALL_LOYALTY_CARDS).size(), is(0));

        assertThat(getModelsOfType(itemList, FILL_BUTTON).size(), is(1));
        assertThat(getModelsOfType(itemList, WALLET_SETTINGS_BUTTON).size(), is(1));
        PropertyModel walletSettingButtonModel =
                getModelsOfType(itemList, WALLET_SETTINGS_BUTTON).get(0);
        assertThat(
                walletSettingButtonModel.get(TEXT_ID),
                is(R.string.autofill_loyalty_card_wallet_settings_button));
        assertThat(getModelsOfType(itemList, FOOTER).size(), is(1));
    }

    @Test
    public void testWalletSettingsButtonRedirectsToSettings() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TOUCH_TO_FILL_LOYALTY_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillLoyaltyCardOutcome.WALLET_SETTINGS);
        mCoordinator.showLoyaltyCards(
                List.of(LOYALTY_CARD_1), List.of(LOYALTY_CARD_1), /* firstTimeUsage= */ true);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, WALLET_SETTINGS_BUTTON).size(), is(1));
        PropertyModel walletSettingButtonModel =
                getModelsOfType(itemList, WALLET_SETTINGS_BUTTON).get(0);
        walletSettingButtonModel.get(ON_CLICK_ACTION).run();

        verify(mDelegateMock).showGoogleWalletSettings();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testShowOneLoyaltyCard() throws TimeoutException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOUCH_TO_FILL_NUMBER_OF_AFFILIATED_LOYALTY_CARDS_SHOWN, 1)
                        .expectIntRecord(TOUCH_TO_FILL_NUMBER_OF_LOYALTY_CARDS_SHOWN, 2)
                        .build();
        mCoordinator.showLoyaltyCards(
                List.of(LOYALTY_CARD_1),
                List.of(LOYALTY_CARD_1, LOYALTY_CARD_2),
                /* firstTimeUsage= */ false);
        histogramWatcher.assertExpected();

        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(HOME_SCREEN));
        assertThat(mTouchToFillPaymentMethodModel.get(FOCUSED_VIEW_ID_FOR_ACCESSIBILITY), is(0));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.ic_globe_24dp));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_loyalty_card_bottom_sheet_title));

        assertThat(getModelsOfType(itemList, LOYALTY_CARD).size(), is(1));
        PropertyModel loyaltyCardModel = itemList.get(1).model;
        assertThat(
                loyaltyCardModel.get(LOYALTY_CARD_NUMBER),
                is(LOYALTY_CARD_1.getLoyaltyCardNumber()));
        assertThat(loyaltyCardModel.get(MERCHANT_NAME), is(LOYALTY_CARD_1.getMerchantName()));

        assertThat(getModelsOfType(itemList, ALL_LOYALTY_CARDS).size(), is(1));
        assertThat(getModelsOfType(itemList, FILL_BUTTON).size(), is(1));
        assertThat(getModelsOfType(itemList, WALLET_SETTINGS_BUTTON).size(), is(0));
        assertThat(getModelsOfType(itemList, FOOTER).size(), is(1));
    }

    @Test
    public void testShowTwoLoyaltyCards() throws TimeoutException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOUCH_TO_FILL_NUMBER_OF_LOYALTY_CARDS_SHOWN, 2)
                        .expectIntRecord(TOUCH_TO_FILL_NUMBER_OF_AFFILIATED_LOYALTY_CARDS_SHOWN, 2)
                        .build();
        mCoordinator.showLoyaltyCards(
                List.of(LOYALTY_CARD_1, LOYALTY_CARD_2),
                List.of(LOYALTY_CARD_1, LOYALTY_CARD_2),
                /* firstTimeUsage= */ false);
        histogramWatcher.assertExpected();

        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(HOME_SCREEN));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);

        assertThat(getModelsOfType(itemList, HEADER).size(), is(1));
        PropertyModel headerModel = itemList.get(0).model;
        assertThat(headerModel.get(IMAGE_DRAWABLE_ID), is(R.drawable.ic_globe_24dp));
        assertThat(
                headerModel.get(TITLE_ID), is(R.string.autofill_loyalty_card_bottom_sheet_title));

        assertThat(getModelsOfType(itemList, LOYALTY_CARD).size(), is(2));
        PropertyModel loyaltyCardModel1 = itemList.get(1).model;
        assertThat(
                loyaltyCardModel1.get(LOYALTY_CARD_NUMBER),
                is(LOYALTY_CARD_1.getLoyaltyCardNumber()));
        assertThat(loyaltyCardModel1.get(MERCHANT_NAME), is(LOYALTY_CARD_1.getMerchantName()));

        PropertyModel loyaltyCardModel2 = itemList.get(2).model;
        assertThat(
                loyaltyCardModel2.get(LOYALTY_CARD_NUMBER),
                is(LOYALTY_CARD_2.getLoyaltyCardNumber()));
        assertThat(loyaltyCardModel2.get(MERCHANT_NAME), is(LOYALTY_CARD_2.getMerchantName()));

        assertThat(getModelsOfType(itemList, ALL_LOYALTY_CARDS).size(), is(1));
        assertThat(getModelsOfType(itemList, FILL_BUTTON).size(), is(0));
        assertThat(getModelsOfType(itemList, WALLET_SETTINGS_BUTTON).size(), is(0));
        assertThat(getModelsOfType(itemList, FOOTER).size(), is(1));
    }

    @Test
    public void testShowAllLoyaltyCards() throws TimeoutException {
        mCoordinator.showLoyaltyCards(
                List.of(LOYALTY_CARD_1),
                List.of(LOYALTY_CARD_1, LOYALTY_CARD_2),
                /* firstTimeUsage= */ false);

        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(HOME_SCREEN));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, ALL_LOYALTY_CARDS).size(), is(1));
        PropertyModel allLoyaltyCardsModel = getModelsOfType(itemList, ALL_LOYALTY_CARDS).get(0);
        // Open the second screen.
        allLoyaltyCardsModel.get(AllLoyaltyCardsItemProperties.ON_CLICK_ACTION).run();

        assertThat(
                mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(ALL_LOYALTY_CARDS_SCREEN));
        assertThat(
                mTouchToFillPaymentMethodModel.get(FOCUSED_VIEW_ID_FOR_ACCESSIBILITY),
                is(
                        org.chromium.chrome.browser.touch_to_fill.payments.R.id
                                .all_loyalty_cards_back_image_button));
        itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(itemList.size(), is(2));

        assertThat(getModelsOfType(itemList, LOYALTY_CARD).size(), is(2));
        PropertyModel loyaltyCardModel1 = itemList.get(0).model;
        assertThat(
                loyaltyCardModel1.get(LOYALTY_CARD_NUMBER),
                is(LOYALTY_CARD_1.getLoyaltyCardNumber()));
        assertThat(loyaltyCardModel1.get(MERCHANT_NAME), is(LOYALTY_CARD_1.getMerchantName()));

        PropertyModel loyaltyCardModel2 = itemList.get(1).model;
        assertThat(
                loyaltyCardModel2.get(LOYALTY_CARD_NUMBER),
                is(LOYALTY_CARD_2.getLoyaltyCardNumber()));
        assertThat(loyaltyCardModel2.get(MERCHANT_NAME), is(LOYALTY_CARD_2.getMerchantName()));

        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        loyaltyCardModel1.get(ON_LOYALTY_CARD_CLICK_ACTION).run();
        verify(mDelegateMock).loyaltyCardSuggestionSelected(LOYALTY_CARD_1);
    }

    @Test
    public void testSelectNonAffiliatedLoyaltyCard() {
        mCoordinator.showLoyaltyCards(
                List.of(LOYALTY_CARD_1),
                List.of(LOYALTY_CARD_1, LOYALTY_CARD_2),
                /* firstTimeUsage= */ false);

        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(HOME_SCREEN));
        // Open the screen with all loyalty cards of a user.
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, ALL_LOYALTY_CARDS).size(), is(1));
        getModelsOfType(itemList, ALL_LOYALTY_CARDS)
                .get(0)
                .get(AllLoyaltyCardsItemProperties.ON_CLICK_ACTION)
                .run();

        // Verify that both loyalty cards are shown.
        assertThat(
                mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(ALL_LOYALTY_CARDS_SCREEN));
        itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOUCH_TO_FILL_ALL_LOYALTY_CARDS_SCREEN_INDEX_SELECTED, 1)
                        .expectIntRecord(
                                TOUCH_TO_FILL_LOYALTY_CARD_OUTCOME_HISTOGRAM,
                                TouchToFillLoyaltyCardOutcome.NON_AFFILIATED_LOYALTY_CARD)
                        .build();

        assertThat(getModelsOfType(itemList, LOYALTY_CARD).size(), is(2));
        PropertyModel loyaltyCardModel = itemList.get(1).model;
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        loyaltyCardModel.get(ON_LOYALTY_CARD_CLICK_ACTION).run();
        verify(mDelegateMock).loyaltyCardSuggestionSelected(LOYALTY_CARD_2);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testPressBackButtonToShowHomeScreen() {
        mCoordinator.showLoyaltyCards(
                List.of(LOYALTY_CARD_1),
                List.of(LOYALTY_CARD_1, LOYALTY_CARD_2),
                /* firstTimeUsage= */ false);

        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(HOME_SCREEN));
        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, LOYALTY_CARD).size(), is(1));

        // Open the screen with all loyalty cards of a user.
        assertThat(getModelsOfType(itemList, ALL_LOYALTY_CARDS).size(), is(1));
        getModelsOfType(itemList, ALL_LOYALTY_CARDS)
                .get(0)
                .get(AllLoyaltyCardsItemProperties.ON_CLICK_ACTION)
                .run();

        // Verify that both loyalty cards are shown.
        assertThat(
                mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(ALL_LOYALTY_CARDS_SCREEN));
        itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, LOYALTY_CARD).size(), is(2));

        // Open the home screen again.
        mTouchToFillPaymentMethodModel.get(BACK_PRESS_HANDLER).run();
        // Verify that only the affiliated loyalty card is shown.
        assertThat(mTouchToFillPaymentMethodModel.get(CURRENT_SCREEN), is(HOME_SCREEN));
        assertThat(
                mTouchToFillPaymentMethodModel.get(FOCUSED_VIEW_ID_FOR_ACCESSIBILITY),
                is(org.chromium.chrome.browser.touch_to_fill.payments.R.id.all_loyalty_cards_item));
        itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, LOYALTY_CARD).size(), is(1));
    }

    @Test
    public void testCallsDelegateWhenLoyaltyCardIsSelected() {
        mCoordinator.showLoyaltyCards(
                List.of(LOYALTY_CARD_1),
                List.of(LOYALTY_CARD_1, LOYALTY_CARD_2),
                /* firstTimeUsage= */ false);
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, LOYALTY_CARD).size(), is(1));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                TOUCH_TO_FILL_AFFILIATED_LOYALTY_CARDS_SCREEN_INDEX_SELECTED, 0)
                        .expectIntRecord(
                                TOUCH_TO_FILL_LOYALTY_CARD_OUTCOME_HISTOGRAM,
                                TouchToFillLoyaltyCardOutcome.AFFILIATED_LOYALTY_CARD)
                        .build();

        PropertyModel loyaltyCardModel = itemList.get(1).model;
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        loyaltyCardModel.get(ON_LOYALTY_CARD_CLICK_ACTION).run();
        verify(mDelegateMock).loyaltyCardSuggestionSelected(LOYALTY_CARD_1);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testManageLoyaltyCardsClick() {
        mCoordinator.showLoyaltyCards(
                List.of(LOYALTY_CARD_1), List.of(LOYALTY_CARD_1), /* firstTimeUsage= */ false);
        assertThat(mTouchToFillPaymentMethodModel.get(VISIBLE), is(true));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TOUCH_TO_FILL_LOYALTY_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillLoyaltyCardOutcome.MANAGE_LOYALTY_CARDS);

        ModelList itemList = mTouchToFillPaymentMethodModel.get(SHEET_ITEMS);
        assertThat(getModelsOfType(itemList, FOOTER).size(), is(1));
        assertThat(
                getModelsOfType(itemList, FOOTER).get(0).get(OPEN_MANAGEMENT_UI_TITLE_ID),
                is(R.string.autofill_bottom_sheet_manage_loyalty_cards));
        getModelsOfType(itemList, FOOTER).get(0).get(OPEN_MANAGEMENT_UI_CALLBACK).run();

        verify(mDelegateMock).openPassesManagementUi();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDismissWithSwipeForLoyaltyCards() {
        mCoordinator.showLoyaltyCards(
                List.of(LOYALTY_CARD_1), List.of(LOYALTY_CARD_1), /* firstTimeUsage= */ false);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TOUCH_TO_FILL_LOYALTY_CARD_OUTCOME_HISTOGRAM,
                        TouchToFillLoyaltyCardOutcome.DISMISS);

        mTouchToFillPaymentMethodModel.get(DISMISS_HANDLER).onResult(StateChangeReason.SWIPE);
        histogramWatcher.assertExpected();
    }

    private static List<PropertyModel> getModelsOfType(ModelList items, int type) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(item -> item.type == type)
                .map(item -> item.model)
                .collect(Collectors.toList());
    }

    private static Optional<PropertyModel> getCardSuggestionModel(
            ModelList items, AutofillSuggestion suggestion) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(
                        item ->
                                item.type == CREDIT_CARD
                                        && item.model.get(MAIN_TEXT).equals(suggestion.getLabel())
                                        && item.model
                                                .get(MINOR_TEXT)
                                                .equals(suggestion.getSecondaryLabel())
                                        && item.model
                                                .get(FIRST_LINE_LABEL)
                                                .equals(suggestion.getSublabel())
                                        && (TextUtils.isEmpty(suggestion.getSecondarySublabel())
                                                || item.model
                                                        .get(SECOND_LINE_LABEL)
                                                        .equals(suggestion.getSecondarySublabel())))
                .findFirst()
                .map(item -> item.model);
    }

    private static Optional<PropertyModel> getBnplSuggestionModel(
            ModelList items, AutofillSuggestion suggestion) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(
                        item ->
                                item.type == BNPL
                                        && item.model
                                                .get(PRIMARY_TEXT)
                                                .equals(suggestion.getLabel())
                                        && item.model
                                                .get(SECONDARY_TEXT)
                                                .equals(suggestion.getSublabel()))
                .findFirst()
                .map(item -> item.model);
    }

    private static Optional<PropertyModel> getBnplIssuerContextModel(
            ModelList items, BnplIssuerContext issuerContext) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(
                        item ->
                                item.type == BNPL_ISSUER
                                        && item.model
                                                .get(ISSUER_NAME)
                                                .equals(issuerContext.getDisplayName())
                                        && item.model
                                                .get(ISSUER_SELECTION_TEXT)
                                                .equals(issuerContext.getSelectionText())
                                        && item.model.get(ISSUER_ICON_ID)
                                                == issuerContext.getIconId()
                                        && item.model.get(ISSUER_LINKED) == issuerContext.isLinked()
                                        && item.model.get(APPLY_ISSUER_DEACTIVATED_STYLE)
                                                != issuerContext.isEligible())
                .findFirst()
                .map(item -> item.model);
    }

    private void assertBnplIssuerContextModelMatches(
            ModelList itemList, BnplIssuerContext expectedBnplIssuerContext) {
        Optional<PropertyModel> bnplIssuerContextModel =
                getBnplIssuerContextModel(itemList, expectedBnplIssuerContext);
        assertTrue(bnplIssuerContextModel.isPresent());
        assertThat(
                bnplIssuerContextModel.get().get(ISSUER_NAME),
                is(expectedBnplIssuerContext.getDisplayName()));
        assertThat(
                bnplIssuerContextModel.get().get(ISSUER_SELECTION_TEXT),
                is(expectedBnplIssuerContext.getSelectionText()));
        assertThat(
                bnplIssuerContextModel.get().get(ISSUER_ICON_ID),
                is(expectedBnplIssuerContext.getIconId()));
        assertThat(
                bnplIssuerContextModel.get().get(ISSUER_LINKED),
                is(expectedBnplIssuerContext.isLinked()));
        assertNotNull(bnplIssuerContextModel.get().get(ON_ISSUER_CLICK_ACTION));
        assertThat(
                bnplIssuerContextModel.get().get(APPLY_ISSUER_DEACTIVATED_STYLE),
                is(!expectedBnplIssuerContext.isEligible()));
    }

    private void assertBnplIssuerSelectionScreenHasCorrectAccessibilityStrings(
            PropertyModel model) {
        assertThat(
                model.get(SHEET_CONTENT_DESCRIPTION_ID),
                is(R.string.autofill_bnpl_issuer_bottom_sheet_content_description));
        assertThat(
                model.get(SHEET_HALF_HEIGHT_DESCRIPTION_ID),
                is(R.string.autofill_bnpl_issuer_bottom_sheet_half_height));
        assertThat(
                model.get(SHEET_FULL_HEIGHT_DESCRIPTION_ID),
                is(R.string.autofill_bnpl_issuer_bottom_sheet_full_height));
        assertThat(
                model.get(SHEET_CLOSED_DESCRIPTION_ID),
                is(R.string.autofill_bnpl_issuer_bottom_sheet_closed));
    }

    private static Optional<PropertyModel> getBnplSelectionProgressFooterModel(ModelList items) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(item -> item.type == BNPL_SELECTION_PROGRESS_FOOTER)
                .findFirst()
                .map(item -> item.model);
    }

    private static void assertFooterModelHasExpectedValues(
            ModelList itemList,
            @StringRes int expectedTermsTextId,
            String expectedHideOptionsLinkText) {
        Optional<PropertyModel> footerModel = getBnplSelectionProgressFooterModel(itemList);
        assertTrue(footerModel.isPresent());
        assertThat(footerModel.get().get(TERMS_TEXT_ID), is(expectedTermsTextId));
        assertThat(footerModel.get().get(HIDE_OPTIONS_LINK_TEXT), is(expectedHideOptionsLinkText));
        assertFalse(footerModel.get().get(APPLY_LINK_DEACTIVATED_STYLE));
        assertNotNull(footerModel.get().get(ON_LINK_CLICK_CALLBACK));
    }

    private static Optional<PropertyModel> getTermsLabelModel(ModelList items) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(item -> item.type == TERMS_LABEL)
                .findFirst()
                .map(item -> item.model);
    }

    private void assertTermsLabelModelHasExpectedTextId(ModelList itemList, @StringRes int textId) {
        Optional<PropertyModel> termsModel = getTermsLabelModel(itemList);
        assertTrue(termsModel.isPresent());
        assertThat(termsModel.get().get(TERMS_LABEL_TEXT_ID), is(textId));
    }

    private static Optional<PropertyModel> getIbanModelByAutofillName(ModelList items, Iban iban) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(
                        item ->
                                item.type == IBAN
                                        && item.model.get(IBAN_VALUE).equals(iban.getLabel()))
                .findFirst()
                .map(item -> item.model);
    }

    private void advanceClockAndClick(PropertyModel cardSuggestionModel) {
        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        cardSuggestionModel.get(ON_CREDIT_CARD_CLICK_ACTION).run();
    }
}
