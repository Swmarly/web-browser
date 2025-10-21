// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_spatial_plane_manager.h"

#include "base/containers/contains.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_spatial_framework_manager.h"
#include "device/vr/openxr/openxr_spatial_utils.h"
#include "device/vr/openxr/openxr_util.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

namespace {
mojom::XRPlaneOrientation ToMojomPlaneOrientation(
    const XrSpatialPlaneAlignmentEXT& alignment) {
  switch (alignment) {
    case XR_SPATIAL_PLANE_ALIGNMENT_HORIZONTAL_UPWARD_EXT:
    case XR_SPATIAL_PLANE_ALIGNMENT_HORIZONTAL_DOWNWARD_EXT:
      return mojom::XRPlaneOrientation::HORIZONTAL;
    case XR_SPATIAL_PLANE_ALIGNMENT_VERTICAL_EXT:
      return mojom::XRPlaneOrientation::VERTICAL;
    default:
      return mojom::XRPlaneOrientation::UNKNOWN;
  }
}
}  // namespace

// static
bool OpenXrSpatialPlaneManager::IsSupported(
    const std::vector<XrSpatialCapabilityEXT>& capabilities) {
  // The only components that we need to support planes are
  // XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT and
  // XR_SPATIAL_COMPONENT_TYPE_PLANE_ALIGNMENT_EXT which are guaranteed to be
  // supported if the XR_SPATIAL_CAPABILITY_ANCHOR_EXT is supported, so that's
  // all we need to check.
  return base::Contains(capabilities, XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT);
}

OpenXrSpatialPlaneManager::OpenXrSpatialPlaneManager(
    const OpenXrExtensionHelper& extension_helper,
    const OpenXrSpatialFrameworkManager& framework_manager)
    : extension_helper_(extension_helper),
      framework_manager_(framework_manager) {}

OpenXrSpatialPlaneManager::~OpenXrSpatialPlaneManager() = default;

void OpenXrSpatialPlaneManager::PopulateCapabilityConfiguration(
    absl::flat_hash_map<XrSpatialCapabilityEXT,
                        absl::flat_hash_set<XrSpatialComponentTypeEXT>>&
        capability_configuration) const {
  capability_configuration[XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT].insert(
      {XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT,
       XR_SPATIAL_COMPONENT_TYPE_PLANE_ALIGNMENT_EXT});
}

void OpenXrSpatialPlaneManager::OnSnapshotChanged() {
  const XrSpatialSnapshotEXT snapshot =
      framework_manager_->GetDiscoverySnapshot();
  if (snapshot == XR_NULL_HANDLE) {
    return;
  }

  // Query the snapshot for all entities that have the necessary plane
  // components.
  XrSpatialComponentDataQueryConditionEXT query_condition{
      XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_CONDITION_EXT};
  std::vector<XrSpatialComponentTypeEXT> component_types = {
      XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT,
      XR_SPATIAL_COMPONENT_TYPE_PLANE_ALIGNMENT_EXT};
  query_condition.componentTypeCount = component_types.size();
  query_condition.componentTypes = component_types.data();

  // First need to query for how many results there are, then we can build the
  // arrays to populate.
  XrSpatialComponentDataQueryResultEXT query_result{
      XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_RESULT_EXT};
  if (XR_FAILED(
          extension_helper_->ExtensionMethods().xrQuerySpatialComponentDataEXT(
              snapshot, &query_condition, &query_result))) {
    return;
  }

  std::vector<XrSpatialEntityIdEXT> entity_ids(
      query_result.entityIdCountOutput);
  query_result.entityIdCapacityInput = entity_ids.size();
  query_result.entityIds = entity_ids.data();

  std::vector<XrSpatialEntityTrackingStateEXT> entity_states(
      query_result.entityIdCountOutput);
  query_result.entityStateCapacityInput = entity_states.size();
  query_result.entityStates = entity_states.data();

  std::vector<XrSpatialPlaneAlignmentEXT> plane_alignments(
      query_result.entityIdCountOutput);
  XrSpatialComponentPlaneAlignmentListEXT plane_alignment_list{
      .type = XR_TYPE_SPATIAL_COMPONENT_PLANE_ALIGNMENT_LIST_EXT,
      .planeAlignmentCount = static_cast<uint32_t>(plane_alignments.size()),
      .planeAlignments = plane_alignments.data()};

  std::vector<XrSpatialBounded2DDataEXT> bounded_2d_data(
      query_result.entityIdCountOutput);
  XrSpatialComponentBounded2DListEXT bounded_2d_list{
      .type = XR_TYPE_SPATIAL_COMPONENT_BOUNDED_2D_LIST_EXT,
      .next = &plane_alignment_list,
      .boundCount = static_cast<uint32_t>(bounded_2d_data.size()),
      .bounds = bounded_2d_data.data()};
  query_result.next = &bounded_2d_list;

  if (XR_FAILED(
          extension_helper_->ExtensionMethods().xrQuerySpatialComponentDataEXT(
              snapshot, &query_condition, &query_result))) {
    return;
  }

  // Reset our list of updated planes. We could potentially be clearing a plane
  // that we said had a pending update but now we don't know about. Since we no
  // longer know about it, then we shouldn't be reporting it.
  updated_entity_ids_.clear();
  absl::flat_hash_set<XrSpatialEntityIdEXT> paused_entity_ids;
  for (uint32_t i = 0; i < query_result.entityIdCountOutput; i++) {
    XrSpatialEntityIdEXT entity_id = entity_ids[i];
    // We don't need to send up any information about stopped planes, and since
    // planes could be subsumed, we'll just process and clear outdated entries
    // every time as well.
    // We'll note paused planes differently. They won't count as updated this
    // frame, but we'll keep them around/existing.
    if (entity_states[i] == XR_SPATIAL_ENTITY_TRACKING_STATE_PAUSED_EXT) {
      paused_entity_ids.insert(entity_id);
      continue;
    }

    if (entity_states[i] != XR_SPATIAL_ENTITY_TRACKING_STATE_TRACKING_EXT) {
      continue;
    }

    // If we don't have an entry for this entity ID, populate the data for it.
    if (!entity_id_to_data_.contains(entity_id)) {
      entity_id_to_data_[entity_id] = mojom::XRPlaneData::New();
    }

    updated_entity_ids_.insert(entity_id);
    mojom::XRPlaneDataPtr& plane_data = entity_id_to_data_[entity_id];

    // Can't use `GetPlaneId` until our entity_id is in the map.
    plane_data->id = GetPlaneId(entity_id);
    plane_data->orientation = ToMojomPlaneOrientation(plane_alignments[i]);

    // The incoming pose has the Z axis as the normal, but WebXR expects the Y
    // axis to be the normal.
    plane_data->mojo_from_plane =
        ZNormalXrPoseToYNormalDevicePose(bounded_2d_data[i].center);

    // For now we don't support polygons, so we just create a rectangle from the
    // extents.
    const auto& extents = bounded_2d_data[i].extents;
    plane_data->polygon.clear();
    plane_data->polygon.push_back(
        mojom::XRPlanePointData::New(-extents.width / 2, -extents.height / 2));
    plane_data->polygon.push_back(
        mojom::XRPlanePointData::New(extents.width / 2, -extents.height / 2));
    plane_data->polygon.push_back(
        mojom::XRPlanePointData::New(extents.width / 2, extents.height / 2));
    plane_data->polygon.push_back(
        mojom::XRPlanePointData::New(-extents.width / 2, extents.height / 2));
  }

  // Remove any planes that are no longer being tracked.
  auto it = entity_id_to_data_.begin();
  while (it != entity_id_to_data_.end()) {
    // If a plane was updated or marked as paused, keep it around. Otherwise,
    // it was either not reported or reported as stopped, so delete it.
    if (updated_entity_ids_.contains(it->first) ||
        paused_entity_ids.contains(it->first)) {
      it++;
    } else {
      entity_id_to_data_.erase(it++);
    }
  }
}

mojom::XRPlaneDetectionDataPtr
OpenXrSpatialPlaneManager::GetDetectedPlanesData() {
  auto planes_data = mojom::XRPlaneDetectionData::New();

  for (const auto& [entity_id, data] : entity_id_to_data_) {
    planes_data->all_planes_ids.push_back(GetPlaneId(entity_id));
    if (updated_entity_ids_.contains(entity_id)) {
      planes_data->updated_planes_data.push_back(data.Clone());
    }
  }

  updated_entity_ids_.clear();
  return planes_data;
}

std::optional<device::Pose> OpenXrSpatialPlaneManager::TryGetMojoFromPlane(
    PlaneId plane_id) const {
  auto it = entity_id_to_data_.find(GetEntityId(plane_id));
  if (it == entity_id_to_data_.end() || !it->second->mojo_from_plane) {
    return std::nullopt;
  }

  return it->second->mojo_from_plane;
}

PlaneId OpenXrSpatialPlaneManager::GetPlaneId(
    XrSpatialEntityIdEXT entity_id) const {
  if (entity_id == XR_NULL_SPATIAL_ENTITY_ID_EXT ||
      !entity_id_to_data_.contains(entity_id)) {
    return kInvalidPlaneId;
  }

  return PlaneId(static_cast<uint64_t>(entity_id));
}

XrSpatialEntityIdEXT OpenXrSpatialPlaneManager::GetEntityId(
    PlaneId plane_id) const {
  if (plane_id == kInvalidPlaneId) {
    return XR_NULL_SPATIAL_ENTITY_ID_EXT;
  }

  auto entity_id = static_cast<XrSpatialEntityIdEXT>(plane_id.GetUnsafeValue());
  if (!entity_id_to_data_.contains(entity_id)) {
    return XR_NULL_SPATIAL_ENTITY_ID_EXT;
  }

  return entity_id;
}

}  // namespace device
