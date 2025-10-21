// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/initial_webui_page_load_metrics_observer.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

InitialWebUIPageLoadMetricsObserver::InitialWebUIPageLoadMetricsObserver() =
    default;

InitialWebUIPageLoadMetricsObserver::~InitialWebUIPageLoadMetricsObserver() =
    default;

const char* InitialWebUIPageLoadMetricsObserver::GetObserverName() const {
  return "InitialWebUIPageLoadMetricsObserver";
}

void InitialWebUIPageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!timing.monotonic_paint_timing ||
      !timing.monotonic_paint_timing->first_paint) {
    return;
  }

  service()->OnFirstPaint(timing.monotonic_paint_timing->first_paint.value());
}

void InitialWebUIPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!timing.monotonic_paint_timing ||
      !timing.monotonic_paint_timing->first_contentful_paint) {
    return;
  }

  service()->OnFirstContentfulPaint(
      timing.monotonic_paint_timing->first_contentful_paint.value());
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
InitialWebUIPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // The target renderer will never be a fenced frame.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
InitialWebUIPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // The target renderer will never be prerendered.
  return STOP_OBSERVING;
}

WaapUIMetricsService* InitialWebUIPageLoadMetricsObserver::service() const {
  CHECK(GetDelegate().GetWebContents()->GetBrowserContext());
  auto* profile = Profile::FromBrowserContext(
      GetDelegate().GetWebContents()->GetBrowserContext());
  // The service is null only if the profile is null or the feature is disabled.
  auto* service = WaapUIMetricsService::Get(profile);
  CHECK(service);
  return service;
}
