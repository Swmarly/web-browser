// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_URL_UTILS_H_
#define COMPONENTS_LENS_LENS_URL_UTILS_H_

#include <array>
#include <map>
#include <string>
#include <vector>

#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_metadata.mojom.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"

class GURL;

namespace lens {

// Query parameter for the search text query.
inline constexpr char kTextQueryParameterKey[] = "q";
// Query parameter for the payload.
inline constexpr char kPayloadQueryParameter[] = "p";
// Query parameter for the translate source language.
inline constexpr char kTranslateSourceQueryParameter[] = "sourcelang";
// Query parameter for the translate target language.
inline constexpr char kTranslateTargetQueryParameter[] = "targetlang";
// Query parameter for the filter type.
inline constexpr char kFilterTypeQueryParameter[] = "filtertype";
inline constexpr char kTranslateFilterTypeQueryParameterValue[] = "tr";
inline constexpr char kLensRequestQueryParameter[] = "vsrid";
inline constexpr char kUnifiedDrillDownQueryParameter[] = "udm";
inline constexpr char kLensSurfaceQueryParameter[] = "lns_surface";

inline constexpr char kContextualVisualInputTypeQueryParameterValue[] = "video";
inline constexpr char kPdfVisualInputTypeQueryParameterValue[] = "pdf";
inline constexpr char kImageVisualInputTypeQueryParameterValue[] = "img";
inline constexpr char kWebpageVisualInputTypeQueryParameterValue[] = "wp";

inline constexpr std::array<lens::MimeType, 3> kUnsupportedVitMimeTypes = {
    lens::MimeType::kVideo, lens::MimeType::kAudio};

// Appends logs to query param as a string
void AppendLogsQueryParam(
    std::string* query_string,
    const std::vector<lens::mojom::LatencyLogPtr>& log_data);

// Returns a query string with all relevant query parameters. Needed for when a
// GURL is unavailable to append to.
std::string GetQueryParametersForLensRequest(EntryPoint ep);

// Returns true if the given URL corresponds to a Lens mWeb result page. This is
// done by checking the URL and its parameters.
bool IsLensMWebResult(const GURL& url);

std::string Base64EncodeRequestId(LensOverlayRequestId request_id);

// Returns the vit query parameter value for the given mime type.
std::string VitQueryParamValueForMimeType(MimeType mime_type);

// Returns the vit query parameter value for the given media type.
std::string VitQueryParamValueForMediaType(
    LensOverlayRequestId::MediaType media_type);

// Returns a key-value map of all parameters in `url` except the query
// parameter.
std::map<std::string, std::string> GetParametersMapWithoutQuery(
    const GURL& url);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_URL_UTILS_H_
