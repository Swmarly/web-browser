// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/metrics/otp_form_event_logger.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

namespace autofill::autofill_metrics {

OtpFormEventLogger::OtpFormEventLogger(BrowserAutofillManager* owner)
    : FormEventLoggerBase("OneTimePassword", owner) {}

OtpFormEventLogger::~OtpFormEventLogger() = default;

void OtpFormEventLogger::OnDidShowSuggestions(
    const FormStructure& form,
    const AutofillField& field,
    base::TimeTicks form_parsed_timestamp,
    bool off_the_record,
    base::span<const Suggestion> suggestions) {
  CHECK(field.Type().GetTypes().contains(ONE_TIME_CODE));
  FormEventLoggerBase::OnDidShowSuggestions(form, field, ONE_TIME_CODE,
                                            form_parsed_timestamp,
                                            off_the_record, suggestions);
}

void OtpFormEventLogger::OnDidFillOtpSuggestion(const FormStructure& form,
                                                const AutofillField& field) {
  has_logged_form_filling_suggestion_filled_ = true;
}

void OtpFormEventLogger::RecordParseForm() {
  base::RecordAction(base::UserMetricsAction("Autofill_ParsedOtpForm"));
}

void OtpFormEventLogger::RecordShowSuggestions() {
  base::RecordAction(base::UserMetricsAction("Autofill_ShowedOtpSuggestions"));
}

bool OtpFormEventLogger::HasLoggedDataToFillAvailable() const {
  return otp_for_filling_existed_;
}

void OtpFormEventLogger::LogUkmInteractedWithForm(
    FormSignature form_signature) {}

void OtpFormEventLogger::OnOtpAvailable() {
  otp_for_filling_existed_ = true;
}

DenseSet<FormTypeNameForLogging>
OtpFormEventLogger::GetSupportedFormTypeNamesForLogging() const {
  return {FormTypeNameForLogging::kOneTimePasswordForm};
}

DenseSet<FormTypeNameForLogging> OtpFormEventLogger::GetFormTypesForLogging(
    const FormStructure& form) const {
  return GetOneTimePasswordTypesForLogging(form);
}

}  // namespace autofill::autofill_metrics
