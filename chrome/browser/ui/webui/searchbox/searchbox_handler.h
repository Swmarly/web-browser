// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_HANDLER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/vector_icon_types.h"

class GURL;
class OmniboxController;
class Profile;
class OmniboxEditModel;

namespace content {
class WebContents;
class WebUIDataSource;
}  // namespace content

namespace searchbox_internal {
// Internal constants for icon resource paths shared by SearchboxHandler and its
// subclasses.
extern const char* kSearchIconResourceName;
}  // namespace searchbox_internal

// How an AIM Composebox query was submitted.
enum class SubmissionType {
  kDefault = 0,
  kDeepSearch = 1,
  kCreateImages = 2,
  kMaxValue = kCreateImages,
};

// Base class for browser-side handlers that handle bi-directional communication
// with WebUI search boxes.
class SearchboxHandler : public searchbox::mojom::PageHandler,
                         public AutocompleteController::Observer {
 public:
  SearchboxHandler(const SearchboxHandler&) = delete;
  SearchboxHandler& operator=(const SearchboxHandler&) = delete;

  static void SetupWebUIDataSource(content::WebUIDataSource* source,
                                   Profile* profile,
                                   bool enable_voice_search = false,
                                   bool enable_lens_search = false);

  // Maps all icons returned from either `AutocompleteMatch::GetVectorIcon()` or
  // `OmniboxAction::GetIconImage()` to svg resource strings.
  virtual std::string AutocompleteIconToResourceName(
      const gfx::VectorIcon& icon) const;

  // Returns true if the page remote is bound and ready to receive calls.
  bool IsRemoteBound() const;

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  // searchbox::mojom::PageHandler:
  void SetPage(
      mojo::PendingRemote<searchbox::mojom::Page> pending_page) override;
  void OnFocusChanged(bool focused) override;
  void QueryAutocomplete(const std::u16string& input,
                         bool prevent_inline_autocomplete) override;
  void StopAutocomplete(bool clear_result) override;
  void OpenAutocompleteMatch(uint8_t line,
                             const GURL& url,
                             bool are_matches_showing,
                             uint8_t mouse_button,
                             bool alt_key,
                             bool ctrl_key,
                             bool meta_key,
                             bool shift_key) override;
  void OnNavigationLikely(
      uint8_t line,
      const GURL& url,
      omnibox::mojom::NavigationPredictor navigation_predictor) override;
  void DeleteAutocompleteMatch(uint8_t line, const GURL& url) override;
  void ActivateKeyword(uint8_t line,
                       const GURL& url,
                       base::TimeTicks match_selection_timestamp,
                       bool is_mouse_event) override;
  void ExecuteAction(uint8_t line,
                     uint8_t action_index,
                     const GURL& url,
                     base::TimeTicks match_selection_timestamp,
                     uint8_t mouse_button,
                     bool alt_key,
                     bool ctrl_key,
                     bool meta_key,
                     bool shift_key) override;
  void GetPlaceholderConfig(GetPlaceholderConfigCallback callback) override;
  void GetRecentTabs(GetRecentTabsCallback callback) override;
  void GetTabPreview(int32_t tab_id, GetTabPreviewCallback callback) override {}
  void NotifySessionStarted() override {}
  void NotifySessionAbandoned() override {}
  void AddFileContext(searchbox::mojom::SelectedFileInfoPtr file_info,
                      mojo_base::BigBuffer file_bytes,
                      AddFileContextCallback callback) override {}
  void AddTabContext(int32_t tab_id, AddTabContextCallback) override {}
  void DeleteContext(const base::UnguessableToken& file_token) override {}
  void ClearFiles() override {}
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key) override {}

  // Stores `callback` to be run when the page remote is bound and ready to
  // receive calls. Runs `callback` immediately if the remote is already bound.
  void set_page_is_bound_callback_for_testing(base::OnceClosure callback);

 protected:
  FRIEND_TEST_ALL_PREFIXES(RealboxHandlerTest, AutocompleteController_Start);
  FRIEND_TEST_ALL_PREFIXES(RealboxHandlerTest, RealboxUpdatesEditModelInput);
  FRIEND_TEST_ALL_PREFIXES(LensSearchboxHandlerTest,
                           Lens_AutocompleteController_Start);
  SearchboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents,
      std::unique_ptr<OmniboxController> controller);
  ~SearchboxHandler() override;

  OmniboxController* omnibox_controller();
  AutocompleteController* autocomplete_controller();
  OmniboxEditModel* edit_model();

  const AutocompleteMatch* GetMatchWithUrl(size_t index, const GURL& url);

  virtual omnibox::ChromeAimToolsAndModels GetAimToolMode();

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<OmniboxController> controller_;
  // Children classes should use `omnibox_controller()` or `controller_`.
  std::unique_ptr<OmniboxController> owned_controller_;

  base::ScopedObservation<AutocompleteController,
                          AutocompleteController::Observer>
      autocomplete_controller_observation_{this};

  mojo::Receiver<searchbox::mojom::PageHandler> page_handler_;
  mojo::Remote<searchbox::mojom::Page> page_;
  base::OnceClosure page_is_bound_callback_for_testing_;

  searchbox::mojom::AutocompleteResultPtr CreateAutocompleteResult(
      const std::u16string& input,
      const AutocompleteResult& result,
      const OmniboxEditModel* edit_model,
      bookmarks::BookmarkModel* bookmark_model,
      const PrefService* prefs,
      const TemplateURLService* turl_service) const;
  base::flat_map<int32_t, searchbox::mojom::SuggestionGroupPtr>
  CreateSuggestionGroupsMap(
      const AutocompleteResult& result,
      const OmniboxEditModel* edit_model,
      const PrefService* prefs,
      const omnibox::GroupConfigMap& suggestion_groups_map) const;
  std::vector<searchbox::mojom::AutocompleteMatchPtr> CreateAutocompleteMatches(
      const AutocompleteResult& result,
      const OmniboxEditModel* edit_model,
      bookmarks::BookmarkModel* bookmark_model,
      const omnibox::GroupConfigMap& suggestion_groups_map,
      const TemplateURLService* turl_service) const;
  virtual std::optional<searchbox::mojom::AutocompleteMatchPtr>
  CreateAutocompleteMatch(const AutocompleteMatch& match,
                          size_t line,
                          const OmniboxEditModel* edit_model,
                          bookmarks::BookmarkModel* bookmark_model,
                          const omnibox::GroupConfigMap& suggestion_groups_map,
                          const TemplateURLService* turl_service) const;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_HANDLER_H_
