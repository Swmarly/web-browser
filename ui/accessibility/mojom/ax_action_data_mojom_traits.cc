// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_action_data_mojom_traits.h"

#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/mojom/ax_tree_id_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<ax::mojom::AXActionDataDataView, ui::AXActionData>::Read(
    ax::mojom::AXActionDataDataView data,
    ui::AXActionData* out) {
  if (!data.ReadAction(&out->action)) {
    return false;
  }
  if (out->action == ax::mojom::Action::kNone) {
    // This might happen with version skew: an action that doesn't
    // have a mapping is converted to the default value, kNone. In this case
    // we cannot process the action properly.
    return false;
  }
  if (!data.ReadTargetTreeId(&out->target_tree_id)) {
    return false;
  }
  if (!data.ReadSourceExtensionId(&out->source_extension_id)) {
    return false;
  }
  ax::mojom::AXNodeIDPtr target_node_id_ptr;
  if (!data.ReadTargetNodeId(&target_node_id_ptr)) {
    return false;
  }
  out->target_node_id = target_node_id_ptr->value;

  if (out->target_node_id != ui::kInvalidAXNodeID &&
      data.target_role() != ax::mojom::Role::kUnknown) {
    // The target could either be found by ID, or by role. Having both set makes
    // no sense.
    return false;
  }
  out->target_role = data.target_role();
  out->request_id = data.request_id();
  out->flags = data.flags();
  ax::mojom::AXNodeIDPtr anchor_node_id_ptr;
  if (!data.ReadAnchorNodeId(&anchor_node_id_ptr)) {
    return false;
  }
  out->anchor_node_id = anchor_node_id_ptr->value;
  out->anchor_offset = data.anchor_offset();

  ax::mojom::AXNodeIDPtr focus_node_id_ptr;
  if (!data.ReadFocusNodeId(&focus_node_id_ptr)) {
    return false;
  }
  out->focus_node_id = focus_node_id_ptr->value;
  out->focus_offset = data.focus_offset();
  out->custom_action_id = data.custom_action_id();
  out->horizontal_scroll_alignment = data.horizontal_scroll_alignment();
  out->vertical_scroll_alignment = data.vertical_scroll_alignment();
  out->scroll_behavior = data.scroll_behavior();
  return data.ReadChildTreeId(&out->child_tree_id) &&
         data.ReadTargetRect(&out->target_rect) &&
         data.ReadTargetPoint(&out->target_point) &&
         data.ReadValue(&out->value) &&
         data.ReadHitTestEventToFire(&out->hit_test_event_to_fire);
}

}  // namespace mojo
