// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_COMPOSEBOX_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_COMPOSEBOX_CONTROLLER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class LensSearchController;

namespace lens {
class LensSessionMetricsLogger;
class LensComposeboxHandler;

// Controller for the Lens compose box. This class is responsible for handling
// communications between the Lens WebUI compose box and other Lens components,
// as well as storing any state needed for the compose box. Note: This class is
// different from the LensSearchboxController, which is responsible for the old,
// non-AIM search box.
class LensComposeboxController {
 public:
  explicit LensComposeboxController(
      LensSearchController* lens_search_controller,
      Profile* profile);
  ~LensComposeboxController();

  // This method is used to set up communication between this instance and the
  // compose box WebUI. This is called by the WebUIController when the WebUI is
  // executing javascript and has bound the handler.
  void BindComposebox(
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler);

  // Issues a composebox query to the side panel results. If this is called when
  // the user is in AIM, issues a follow up query. Otherwise, issues a new AIM
  // session query.
  void IssueComposeboxQuery(const std::string& query_text);

  // Called when the focus state of the composebox changes.
  void OnFocusChanged(bool focused);

  // Cleans up any any state associated with this UI instance.
  void CloseUI();

  // Handles AIM messages from the side panel remote UI.
  void OnAimMessage(const std::vector<uint8_t>& message);

  // Resets data associated with the handshake. This allows the controller
  // to know when communication is established with AIM.
  void ResetAimHandshake();

  // Shows the Lens selection overlay. A no-op if it is already open.
  void ShowLensSelectionOverlay();

  // Returns the session metrics logger for the current Lens session.
  LensSessionMetricsLogger* GetSessionMetricsLogger();

  LensComposeboxHandler* composebox_handler_for_testing() {
    return composebox_handler_.get();
  }

  const lens::proto::LensOverlaySuggestInputs&
  get_raw_suggest_inputs_for_testing() const {
    return suggest_inputs_;
  }

  lens::proto::LensOverlaySuggestInputs GetLensSuggestInputs() const;

  void UpdateSuggestInputs(
      const lens::proto::LensOverlaySuggestInputs& suggest_inputs);

 private:
  // Builds a SubmitQuery ClientToAimMessage message to send to the side panel
  // remote UI.
  lens::ClientToAimMessage BuildSubmitQueryMessage(
      const std::string& query_text);

  // Owns this.
  const raw_ptr<LensSearchController> lens_search_controller_;

  // Guarantee to outlive this.
  const raw_ptr<Profile> profile_;

  // The remote UI's capabilities. Only populated once the handshake completes.
  std::set<lens::FeatureCapability> remote_ui_capabilities_;

  // A query that was issued before the remote UI was ready. This will be sent
  // once the handshake completes.
  std::optional<std::string> pending_query_text_;

  // The class responsible for handling messages between the compose box and
  // the WebUI.
  std::unique_ptr<LensComposeboxHandler> composebox_handler_;

  // The current suggest inputs. The fields in this proto are updated
  // whenever new data is available (i.e. after an objects or interaction
  // response is received)
  lens::proto::LensOverlaySuggestInputs suggest_inputs_;
};
}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_COMPOSEBOX_CONTROLLER_H_
