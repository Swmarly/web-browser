// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_switches.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"

namespace switches {

#if BUILDFLAG(IS_ANDROID)
// Mitigate overlap cases between the legacy search engine promo and the
// device-based program eligibility determinations.
BASE_FEATURE(kMitigateLegacySearchEnginePromoOverlap,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRestrictLegacySearchEnginePromoOnFormFactors,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kResolveRegionalCapabilitiesFromDevice,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kUseFinchPermanentCountryForFetchCountryId,
             "UseFinchPermanentCountyForFetchCountryId",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kTaiyaki,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

namespace {
constexpr base::FeatureParam<RegionalCapabilitiesChoiceScreenSurface>::Option
    kChoiceScreenSurfaceOptions[] = {
        {RegionalCapabilitiesChoiceScreenSurface::kAll, "all"},
        {RegionalCapabilitiesChoiceScreenSurface::kInFreOnly, "fre_only"}};
}  // namespace

const base::FeatureParam<RegionalCapabilitiesChoiceScreenSurface>
    kTaiyakiChoiceScreenSurface{
        &kTaiyaki, "choice_screen_surface",
        RegionalCapabilitiesChoiceScreenSurface::kAll,
        &kChoiceScreenSurfaceOptions};
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

BASE_FEATURE(kDynamicProfileCountry,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

}  // namespace switches
