// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/features.h"

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "net/base/cronet_buildflags.h"
#include "net/disk_cache/buildflags.h"
#include "net/net_buildflags.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_constants.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace net::features {

BASE_FEATURE(kAlpsForHttp2, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAvoidH2Reprioritization, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCapReferrerToOriginOnCrossOrigin,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAsyncDns,
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kDnsTransactionDynamicTimeouts, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<double> kDnsTransactionTimeoutMultiplier{
    &kDnsTransactionDynamicTimeouts, "DnsTransactionTimeoutMultiplier", 7.5};

const base::FeatureParam<base::TimeDelta> kDnsMinTransactionTimeout{
    &kDnsTransactionDynamicTimeouts, "DnsMinTransactionTimeout",
    base::Seconds(12)};

BASE_FEATURE(kUseDnsHttpsSvcb, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool> kUseDnsHttpsSvcbEnforceSecureResponse{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbEnforceSecureResponse", false};

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbInsecureExtraTimeMax{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbInsecureExtraTimeMax",
    base::Milliseconds(50)};

const base::FeatureParam<int> kUseDnsHttpsSvcbInsecureExtraTimePercent{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbInsecureExtraTimePercent", 20};

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbInsecureExtraTimeMin{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbInsecureExtraTimeMin",
    base::Milliseconds(5)};

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbSecureExtraTimeMax{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbSecureExtraTimeMax",
    base::Milliseconds(50)};

const base::FeatureParam<int> kUseDnsHttpsSvcbSecureExtraTimePercent{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbSecureExtraTimePercent", 20};

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbSecureExtraTimeMin{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbSecureExtraTimeMin",
    base::Milliseconds(5)};

BASE_FEATURE(kUseHostResolverCache, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHappyEyeballsV3, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kAlternativePortForGloballyReachableCheck{
    &kUseAlternativePortForGloballyReachableCheck,
    "AlternativePortForGloballyReachableCheck", 443};

BASE_FEATURE(kUseAlternativePortForGloballyReachableCheck,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableIPv6ReachabilityOverride,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMaintainConnectionsOnIpv6TempAddrChange,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableTLS13EarlyData, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNetworkQualityEstimator, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kRecentHTTPThresholdInSeconds{
    &kNetworkQualityEstimator, "RecentHTTPThresholdInSeconds", -1};
const base::FeatureParam<int> kRecentTransportThresholdInSeconds{
    &kNetworkQualityEstimator, "RecentTransportThresholdInSeconds", -1};
const base::FeatureParam<int> kRecentEndToEndThresholdInSeconds{
    &kNetworkQualityEstimator, "RecentEndToEndThresholdInSeconds", -1};
const base::FeatureParam<int> kCountNewObservationsReceivedComputeEct{
    &kNetworkQualityEstimator, "CountNewObservationsReceivedComputeEct", 50};
const base::FeatureParam<int> kObservationBufferSize{
    &kNetworkQualityEstimator, "ObservationBufferSize", 300};
const base::FeatureParam<base::TimeDelta>
    kEffectiveConnectionTypeRecomputationInterval{
        &kNetworkQualityEstimator,
        "EffectiveConnectionTypeRecomputationInterval", base::Seconds(10)};

BASE_FEATURE(kSplitCacheByIncludeCredentials,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSplitCacheByNetworkIsolationKey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Note: Use of this feature is gated on the HTTP cache itself being
// partitioned, which is controlled by the kSplitCacheByNetworkIsolationKey
// feature.
BASE_FEATURE(kSplitCacheByCrossSiteMainFrameNavigationBoolean,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSplitCodeCacheByNetworkIsolationKey,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPartitionConnectionsByNetworkIsolationKey,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrefixCookieHttp, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrefixCookieHostHttp, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSearchEnginePreconnectInterval,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSearchEnginePreconnect2, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kIdleTimeoutInSeconds,
                   &kSearchEnginePreconnect2,
                   "IdleTimeoutInSeconds",
                   30);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kShortSessionThreshold,
                   &kSearchEnginePreconnect2,
                   "MaxShortSessionThreshold",
                   base::Seconds(30));

extern const base::FeatureParam<int> kMaxPreconnectRetryInterval(
    &kSearchEnginePreconnect2,
    "MaxPreconnectRetryInterval",
    30);

BASE_FEATURE_PARAM(int,
                   kPingIntervalInSeconds,
                   &kSearchEnginePreconnect2,
                   "PingIntervalInSeconds",
                   27);

BASE_FEATURE_PARAM(std::string,
                   kQuicConnectionOptions,
                   &kSearchEnginePreconnect2,
                   "QuicConnectionOptions",
                   "ECCP");

BASE_FEATURE_PARAM(bool,
                   kFallbackInLowPowerMode,
                   &kSearchEnginePreconnect2,
                   "FallbackInLowPowerMode",
                   false);

BASE_FEATURE(kShortLaxAllowUnsafeThreshold, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSameSiteDefaultChecksMethodRigorously,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTimeoutTcpConnectAttempt, base::FEATURE_DISABLED_BY_DEFAULT);

extern const base::FeatureParam<double> kTimeoutTcpConnectAttemptRTTMultiplier(
    &kTimeoutTcpConnectAttempt,
    "TimeoutTcpConnectAttemptRTTMultiplier",
    5.0);

extern const base::FeatureParam<base::TimeDelta> kTimeoutTcpConnectAttemptMin(
    &kTimeoutTcpConnectAttempt,
    "TimeoutTcpConnectAttemptMin",
    base::Seconds(8));

extern const base::FeatureParam<base::TimeDelta> kTimeoutTcpConnectAttemptMax(
    &kTimeoutTcpConnectAttempt,
    "TimeoutTcpConnectAttemptMax",
    base::Seconds(30));

BASE_FEATURE(kCookieSameSiteConsidersRedirectChain,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowSameSiteNoneCookiesInSandbox,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWaitForFirstPartySetsInit, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the maximum time duration an outermost frame navigation should be
// deferred by RWS initialization.
extern const base::FeatureParam<base::TimeDelta>
    kWaitForFirstPartySetsInitNavigationThrottleTimeout{
        &kWaitForFirstPartySetsInit,
        "kWaitForFirstPartySetsInitNavigationThrottleTimeout",
        base::Seconds(0)};

BASE_FEATURE(kRequestStorageAccessNoCorsRequired,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStorageAccessApiFollowsSameOriginPolicy,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStaticKeyPinningEnforcement, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCookieDomainRejectNonASCII, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables partitioning of third party storage (IndexedDB, CacheStorage, etc.)
// by the top level site to reduce fingerprinting.
BASE_FEATURE(kThirdPartyStoragePartitioning, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTpcdTrialSettings,
             "TpcdSupportSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTopLevelTpcdTrialSettings,
             "TopLevelTpcdSupportSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTpcdMetadataGrants, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTpcdMetadataStageControl, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAlpsParsing, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAlpsClientHintParsing, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShouldKillSessionOnAcceptChMalformed,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableWebsocketsOverHttp3, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Disabled because of https://crbug.com/1489696.
BASE_FEATURE(kEnableGetNetworkConnectivityHintAPI,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTcpPortRandomizationWin, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kTcpPortRandomizationWinVersionMinimum,
                   &kTcpPortRandomizationWin,
                   "TcpPortRandomizationWinVersionMinimum",
                   static_cast<int>(base::win::Version::WIN11_22H2));

BASE_FEATURE(kTcpPortReuseMetricsWin, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTcpSocketIoCompletionPortWin, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kAvoidEntryCreationForNoStore, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kAvoidEntryCreationForNoStoreCacheSize{
    &kAvoidEntryCreationForNoStore, "AvoidEntryCreationForNoStoreCacheSize",
    1000};

// A flag for new Kerberos feature, that suggests new UI
// when Kerberos authentication in browser fails on ChromeOS.
// b/260522530
#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kKerberosInBrowserRedirect, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// A flag to use asynchronous session creation for new QUIC sessions.
BASE_FEATURE(kAsyncQuicSession,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// A flag to make multiport context creation asynchronous.
BASE_FEATURE(kAsyncMultiPortPath,
#if !BUILDFLAG(CRONET_BUILD) && (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID))
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Probabilistic reveal tokens configuration settings
BASE_FEATURE(kEnableProbabilisticRevealTokens,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kProbabilisticRevealTokenServer{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"ProbabilisticRevealTokenServer",
    /*default_value=*/"https://aaftokenissuer.pa.googleapis.com"};

const base::FeatureParam<std::string> kProbabilisticRevealTokenServerPath{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"ProbabilisticRevealTokenServerPath",
    /*default_value=*/"/v1/issueprts"};

const base::FeatureParam<bool> kBypassProbabilisticRevealTokenRegistry{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"BypassProbabilisticRevealTokenRegistry",
    /*default_value=*/false};

const base::FeatureParam<bool> kUseCustomProbabilisticRevealTokenRegistry{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"UseCustomProbabilisticRevealTokenRegistry",
    /*default_value=*/false};

const base::FeatureParam<std::string> kCustomProbabilisticRevealTokenRegistry{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"CustomProbabilisticRevealTokenRegistry",
    /*default_value=*/""};

const base::FeatureParam<bool> kProbabilisticRevealTokensOnlyInIncognito{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"ProbabilisticRevealTokensOnlyInIncognito",
    /*default_value=*/false};

const base::FeatureParam<bool> kProbabilisticRevealTokenFetchOnly{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"ProbabilisticRevealTokenFetchOnly",
    /*default_value=*/false};

const base::FeatureParam<bool>
    kEnableProbabilisticRevealTokensForNonProxiedRequests{
        &kEnableProbabilisticRevealTokens,
        /*name=*/"EnableProbabilisticRevealTokensForNonProxiedRequests",
        /*default_value=*/false};

const base::FeatureParam<bool>
    kProbabilisticRevealTokensAddHeaderToProxiedRequests{
        &kEnableProbabilisticRevealTokens,
        /*name=*/"ProbabilisticRevealTokensAddHeaderToProxiedRequests",
        /*default_value=*/false};

// IP protection experiment configuration settings
BASE_FEATURE(kEnableIpProtectionProxy,
             "EnableIpPrivacyProxy",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kIpPrivacyTokenServer{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyTokenServer",
    /*default_value=*/"https://prod.ipprotectionauth.goog"};

const base::FeatureParam<std::string> kIpPrivacyTokenServerGetInitialDataPath{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyTokenServerGetInitialDataPath",
    /*default_value=*/"/v1/ipblinding/getInitialData"};

const base::FeatureParam<std::string> kIpPrivacyTokenServerGetTokensPath{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyTokenServerGetTokensPath",
    /*default_value=*/"/v1/ipblinding/auth"};

const base::FeatureParam<std::string> kIpPrivacyTokenServerGetProxyConfigPath{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyTokenServerGetProxyConfigPath",
    /*default_value=*/"/v1/ipblinding/getProxyConfig"};

const base::FeatureParam<int> kIpPrivacyAuthTokenCacheBatchSize{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyAuthTokenCacheBatchSize",
    /*default_value=*/64};

const base::FeatureParam<int> kIpPrivacyAuthTokenCacheLowWaterMark{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyAuthTokenCacheLowWaterMark",
    /*default_value=*/16};

const base::FeatureParam<base::TimeDelta> kIpPrivacyProxyListFetchInterval{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyProxyListFetchInterval",
    /*default_value=*/base::Hours(1)};

const base::FeatureParam<base::TimeDelta> kIpPrivacyProxyListMinFetchInterval{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyProxyMinListFetchInterval",
    /*default_value=*/base::Minutes(1)};

const base::FeatureParam<base::TimeDelta> kIpPrivacyProxyListFetchIntervalFuzz{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyProxyListFetchIntervalFuzz",
    /*default_value=*/base::Minutes(30)};

const base::FeatureParam<bool> kIpPrivacyDirectOnly{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyDirectOnly",
    /*default_value=*/false};

const base::FeatureParam<bool> kIpPrivacyIncludeOAuthTokenInGetProxyConfig{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyIncludeOAuthTokenInGetProxyConfig",
    /*default_value=*/false};

const base::FeatureParam<std::string> kIpPrivacyProxyAHostnameOverride{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyProxyAHostnameOverride",
    /*default_value=*/""};

const base::FeatureParam<std::string> kIpPrivacyProxyBHostnameOverride{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyProxyBHostnameOverride",
    /*default_value=*/""};

const base::FeatureParam<bool> kIpPrivacyAddHeaderToProxiedRequests{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyAddHeaderToProxiedRequests",
    /*default_value=*/false};

const base::FeatureParam<base::TimeDelta> kIpPrivacyExpirationFuzz{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyExpirationFuzz",
    /*default_value=*/base::Minutes(15)};

const base::FeatureParam<base::TimeDelta>
    kIpPrivacyTryGetAuthTokensNotEligibleBackoff{
        &kEnableIpProtectionProxy,
        /*name=*/"IpPrivacyTryGetAuthTokensNotEligibleBackoff",
        /*default_value=*/base::Hours(1)};

const base::FeatureParam<base::TimeDelta>
    kIpPrivacyTryGetAuthTokensTransientBackoff{
        &kEnableIpProtectionProxy,
        /*name=*/"IpPrivacyTryGetAuthTokensTransientBackoff",
        /*default_value=*/base::Seconds(5)};

const base::FeatureParam<base::TimeDelta> kIpPrivacyTryGetAuthTokensBugBackoff{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyTryGetAuthTokensBugBackoff",
    /*default_value=*/base::Minutes(10)};

const base::FeatureParam<double> kIpPrivacyBackoffJitter{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyBackoffJitter",
    /*default_value=*/0.25};

const base::FeatureParam<bool> kIpPrivacyRestrictTopLevelSiteSchemes{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyRestrictTopLevelSiteSchemes",
    /*default_value=*/true};

const base::FeatureParam<bool> kIpPrivacyUseQuicProxies{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyUseQuicProxies",
    /*default_value=*/false};

const base::FeatureParam<bool> kIpPrivacyUseQuicProxiesOnly{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyUseQuicProxiesOnly",
    /*default_value=*/false};

const base::FeatureParam<bool>
    kIpPrivacyUseQuicProxiesWithoutWaitingForConnectResponse{
        &kEnableIpProtectionProxy,
        /*name=*/"IpPrivacyUseQuicProxiesWithoutWaitingForConnectResponse",
        /*default_value=*/false};

const base::FeatureParam<bool> kIpPrivacyFallbackToDirect{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyFallbackToDirect",
    /*default_value=*/true};

const base::FeatureParam<int> kIpPrivacyDebugExperimentArm{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyDebugExperimentArm",
    /*default_value=*/0};

const base::FeatureParam<bool> kIpPrivacyAlwaysCreateCore{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyAlwaysCreateCore",
    /*default_value=*/false};

const base::FeatureParam<bool> kIpPrivacyOnlyInIncognito{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyOnlyInIncognito",
    /*default_value=*/false};

const base::FeatureParam<bool> kIpPrivacyEnableUserBypass{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyEnableUserBypass",
    /*default_value=*/false};

const base::FeatureParam<bool> kIpPrivacyDisableForEnterpriseByDefault{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyDisableForEnterpriseByDefault",
    /*default_value=*/false};

const base::FeatureParam<bool> kIpPrivacyEnableIppInDevTools{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyEnableIppInDevTools",
    /*default_value=*/true};

const base::FeatureParam<bool> kIpPrivacyEnableIppPanelInDevTools{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyEnableIppPanelInDevTools",
    /*default_value=*/false};

const base::FeatureParam<std::string> kIpPrivacyUnconditionalProxyDomainList{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyUnconditionalProxyDomainList", /*default_value=*/""};

BASE_FEATURE(kEnableIpPrivacyProxyAdvancedFallbackLogic,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExcludeLargeBodyReports,
             "ExcludeLargeReportBodies",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kMaxReportBodySizeKB,
                   &kExcludeLargeBodyReports,
                   "max_report_body_size_kb",
                   1024);

BASE_FEATURE(kRelatedWebsitePartitionAPI, base::FEATURE_DISABLED_BY_DEFAULT);

// Network-change migration requires NetworkHandle support, which are currently
// only supported on Android (see
// NetworkChangeNotifier::AreNetworkHandlesSupported).
#if BUILDFLAG(IS_ANDROID)
inline constexpr auto kMigrateSessionsOnNetworkChangeV2Default =
    base::FEATURE_ENABLED_BY_DEFAULT;
#else   // !BUILDFLAG(IS_ANDROID)
inline constexpr auto kMigrateSessionsOnNetworkChangeV2Default =
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif  // BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMigrateSessionsOnNetworkChangeV2,
             kMigrateSessionsOnNetworkChangeV2Default);

#if BUILDFLAG(IS_LINUX)
BASE_FEATURE(kAddressTrackerLinuxIsProxied, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX)

// Enables binding of cookies to the port that originally set them by default.
BASE_FEATURE(kEnablePortBoundCookies, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables binding of cookies to the scheme that originally set them.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableSchemeBoundCookies);
BASE_FEATURE(kEnableSchemeBoundCookies, base::FEATURE_DISABLED_BY_DEFAULT);

// Disallows cookies to have non ascii values in their name or value.
NET_EXPORT BASE_DECLARE_FEATURE(kDisallowNonAsciiCookies);
BASE_FEATURE(kDisallowNonAsciiCookies, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTimeLimitedInsecureCookies, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable third-party cookie blocking from the command line.
BASE_FEATURE(kForceThirdPartyCookieBlocking,
             "ForceThirdPartyCookieBlockingEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableEarlyHintsOnHttp11, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableWebTransportDraft07, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebTransportFineGrainedThrottling,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, partitioned storage will be allowed even if third-party cookies
// are disabled by default. Partitioned storage will not be allowed if
// third-party cookies are disabled due to a specific rule.
BASE_FEATURE(kThirdPartyPartitionedStorageAllowedByDefault,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSpdyHeadersToHttpResponseUseBuilder,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseNewAlpsCodepointHttp2, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseNewAlpsCodepointQUIC, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTruncateBodyToContentLength, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kReduceIPAddressChangeNotification,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kUseNetworkPathMonitorForNetworkChangeNotifier,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

BASE_FEATURE(kDeviceBoundSessions, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPersistDeviceBoundSessions, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kDeviceBoundSessionsRequireOriginTrialTokens,
                   &kDeviceBoundSessions,
                   "RequireOriginTrialTokens",
                   true);
BASE_FEATURE_PARAM(bool,
                   kDeviceBoundSessionsRefreshQuota,
                   &kDeviceBoundSessions,
                   "RefreshQuota",
                   true);
BASE_FEATURE_PARAM(bool,
                   kDeviceBoundSessionsCheckSubdomainRegistration,
                   &kDeviceBoundSessions,
                   "CheckSubdomainRegistration",
                   true);
BASE_FEATURE_PARAM(int,
                   kDeviceBoundSessionsSchemaVersion,
                   &kDeviceBoundSessions,
                   "SchemaVersion",
                   2);
BASE_FEATURE_PARAM(bool,
                   kDeviceBoundSessionsOriginTrialFeedback,
                   &kDeviceBoundSessions,
                   "OriginTrialFeedback",
                   true);

BASE_FEATURE(kDeviceBoundSessionsFederatedRegistration,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kDeviceBoundSessionsFederatedRegistrationCheckWellKnown,
                   &kDeviceBoundSessionsFederatedRegistration,
                   "CheckWellKnown",
                   true);

BASE_FEATURE(kSpdySessionForProxyAdditionalChecks,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCompressionDictionaryTransportRequireKnownRootCert,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReportingApiEnableEnterpriseCookieIssues,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSimdutfBase64Support,
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kFurtherOptimizeParsingDataUrls, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNoVarySearchIgnoreUnrecognizedKeys,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableStaticCTAPIEnforcement, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDiskCacheBackendExperiment, base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<DiskCacheBackend>::Option
    kDiskCacheBackendOptions[] = {
        {DiskCacheBackend::kDefault, "default"},
        {DiskCacheBackend::kSimple, "simple"},
        {DiskCacheBackend::kBlockfile, "blockfile"},
#if BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND)
        {DiskCacheBackend::kSql, "sql"},
#endif  // ENABLE_DISK_CACHE_SQL_BACKEND
};
const base::FeatureParam<DiskCacheBackend> kDiskCacheBackendParam{
    &kDiskCacheBackendExperiment, "backend",
#if BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND)
    DiskCacheBackend::kSql,
#else   // ENABLE_DISK_CACHE_SQL_BACKEND
    DiskCacheBackend::kDefault,
#endif  // ENABLE_DISK_CACHE_SQL_BACKEND
    &kDiskCacheBackendOptions};

#if BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND)
BASE_FEATURE_PARAM(int,
                   kSqlDiskCacheForceCheckpointThreshold,
                   &kDiskCacheBackendExperiment,
                   "SqlDiskCacheForceCheckpointThreshold",
                   20000);
BASE_FEATURE_PARAM(int,
                   kSqlDiskCacheIdleCheckpointThreshold,
                   &kDiskCacheBackendExperiment,
                   "SqlDiskCacheIdleCheckpointThreshold",
                   1000);
BASE_FEATURE_PARAM(int,
                   kSqlDiskCacheOptimisticWriteBufferSize,
                   &kDiskCacheBackendExperiment,
                   "SqlDiskCacheOptimisticWriteBufferSize",
                   32 * 1024 * 1024);
BASE_FEATURE_PARAM(bool,
                   kSqlDiskCacheSynchronousOff,
                   &kDiskCacheBackendExperiment,
                   "SqlDiskCacheSynchronousOff",
                   false);
#endif  // ENABLE_DISK_CACHE_SQL_BACKEND

BASE_FEATURE(kIgnoreHSTSForLocalhost, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSimpleCachePrioritizedCaching, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int>
    kSimpleCachePrioritizedCachingPrioritizationFactor{
        &kSimpleCachePrioritizedCaching,
        /*name=*/"SimpleCachePrioritizedCachingPrioritizationFactor",
        /*default_value=*/10};

const base::FeatureParam<base::TimeDelta>
    kSimpleCachePrioritizedCachingPrioritizationPeriod{
        &kSimpleCachePrioritizedCaching,
        /*name=*/"SimpleCachePrioritizedCachingPrioritizationPeriod",
        /*default_value=*/base::Days(1)};

BASE_FEATURE(kHstsTopLevelNavigationsOnly, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kHttpCacheMappedFileFlushWin, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kHttpCacheNoVarySearch,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE_PARAM(size_t,
                   kHttpCacheNoVarySearchCacheMaxEntries,
                   &kHttpCacheNoVarySearch,
                   "max_entries",
                   1000);

BASE_FEATURE_PARAM(bool,
                   kHttpCacheNoVarySearchPersistenceEnabled,
                   &kHttpCacheNoVarySearch,
                   "persistence_enabled",
                   true);

BASE_FEATURE_PARAM(bool,
                   kHttpCacheNoVarySearchKeepNotSuitable,
                   &kHttpCacheNoVarySearch,
                   "keep_not_suitable",
                   true);

BASE_FEATURE(kHttpNoVarySearchDataUseNewAreEquivalent,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kHttpNoVarySearchDataAreEquivalentCheckResult,
                   &kHttpNoVarySearchDataUseNewAreEquivalent,
                   "check_result",
                   false);

BASE_FEATURE(kReportingApiCorsOriginHeader, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kUseCertTransparencyAwareApiForOsCertVerify,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSelfSignedLocalNetworkInterstitial,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
BASE_FEATURE(kVerifyQWACs, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

BASE_FEATURE(kRestrictAbusePorts, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRestrictAbusePortsOnLocalhost, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTLSTrustAnchorIDs, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSocketPoolSizePerTopLevelSiteTrial,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kSocketPoolSizePerTopLevelSiteTrialNormalProfileLimit,
                   &kSocketPoolSizePerTopLevelSiteTrial,
                   "SocketPoolSizePerTopLevelSiteTrialNormalProfileLimit",
                   256);

BASE_FEATURE_PARAM(int,
                   kSocketPoolSizePerTopLevelSiteTrialWebSocketProfileLimit,
                   &kSocketPoolSizePerTopLevelSiteTrial,
                   "SocketPoolSizePerTopLevelSiteTrialWebSocketProfileLimit",
                   256);

BASE_FEATURE(kNetTaskScheduler, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kNetTaskSchedulerHttpProxyConnectJob,
                   &kNetTaskScheduler,
                   "http_proxy_connect_job",
                   true);
BASE_FEATURE_PARAM(bool,
                   kNetTaskSchedulerHttpCache,
                   &kNetTaskScheduler,
                   "http_cache",
                   false);
BASE_FEATURE_PARAM(bool,
                   kNetTaskSchedulerHttpCacheTransaction,
                   &kNetTaskScheduler,
                   "http_cache_transaction",
                   false);
BASE_FEATURE_PARAM(bool,
                   kNetTaskSchedulerHttpStreamFactoryJob,
                   &kNetTaskScheduler,
                   "http_stream_factory_job",
                   true);
BASE_FEATURE_PARAM(bool,
                   kNetTaskSchedulerHttpStreamFactoryJobController,
                   &kNetTaskScheduler,
                   "http_stream_factory_job_controller",
                   true);
BASE_FEATURE_PARAM(bool,
                   kNetTaskSchedulerURLRequestErrorJob,
                   &kNetTaskScheduler,
                   "url_request_error_job",
                   true);
BASE_FEATURE_PARAM(bool,
                   kNetTaskSchedulerURLRequestHttpJob,
                   &kNetTaskScheduler,
                   "url_request_http_job",
                   true);
BASE_FEATURE_PARAM(bool,
                   kNetTaskSchedulerURLRequestJob,
                   &kNetTaskScheduler,
                   "url_request_job",
                   true);
BASE_FEATURE_PARAM(bool,
                   kNetTaskSchedulerURLRequestRedirectJob,
                   &kNetTaskScheduler,
                   "url_request_redirect_job",
                   true);

BASE_FEATURE(kAdditionalDelayMainJob, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kAdditionalDelay,
                   &kAdditionalDelayMainJob,
                   "AdditionalDelay",
                   base::Milliseconds(0));
BASE_FEATURE_PARAM(bool,
                   kDelayMainJobWithAvailableSpdySession,
                   &kAdditionalDelayMainJob,
                   "DelayMainJobWithAvailableSpdySession",
                   false);

BASE_FEATURE(kExtendQuicHandshakeTimeout, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kQuicHandshakeTimeout,
                   &kExtendQuicHandshakeTimeout,
                   "QuicHandshakeTimeout",
                   base::Seconds(quic::kMaxTimeForCryptoHandshakeSecs));

BASE_FEATURE(kQuicLongerIdleConnectionTimeout,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLowerQuicMaxPacketSize, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(size_t,
                   kQuicMaxPacketSize,
                   &kLowerQuicMaxPacketSize,
                   "mtu",
                   quic::kDefaultMaxPacketSize);

BASE_FEATURE(kConfigureQuicHints, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kQuicHintHostPortPairs,
                   &kConfigureQuicHints,
                   /*name=*/"quic_hints",
                   /*default_value=*/"");
BASE_FEATURE_PARAM(std::string,
                   kWildcardQuicHintHostPortPairs,
                   &kConfigureQuicHints,
                   /*name=*/"wildcard_quic_hints",
                   /*default_value=*/"");

BASE_FEATURE(kDnsFilteringDetails, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUpdateIsMainFrameOriginRecentlyAccessed,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kRecentlyAccessedOriginCacheSize,
                   &kUpdateIsMainFrameOriginRecentlyAccessed,
                   "cache_size",
                   64);

BASE_FEATURE(kTryQuicByDefault, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kQuicOptions,
                   &kTryQuicByDefault,
                   "quic_options",
                   "");

BASE_FEATURE(kDnsResponseDiscardPartialQuestions,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace net::features
