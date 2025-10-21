// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PERSIST_TAB_CONTEXT_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PERSIST_TAB_CONTEXT_BROWSER_AGENT_H_

#import "base/scoped_observation.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_state_agent.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
#import "ios/web/public/web_state_observer.h"

namespace web {
class WebState;
}  // namespace web

@class PersistTabContextStateObserver;

// PersistTabContextBrowserAgent allows saving and retrieving saved page
// contexts. Page contexts are retrieved and saved to storage when a tab is
// backgrounded (either switched tab or closed app). The page contexts that are
// stored contain information on tab content such as the APC and the inner_text,
// along with the page title and url. Once a tab is closed, its page context is
// deleted from storage.
class PersistTabContextBrowserAgent
    : public BrowserUserData<PersistTabContextBrowserAgent>,
      public web::WebStateObserver,
      public TabsDependencyInstaller {
 public:
  PersistTabContextBrowserAgent(const PersistTabContextBrowserAgent&) = delete;
  PersistTabContextBrowserAgent& operator=(
      const PersistTabContextBrowserAgent&) = delete;

  ~PersistTabContextBrowserAgent() override;

  // Type alias for the result map of GetContextsAsync. Maps each webstate
  // unique id to an optional unique pointer holding the fetched
  // page context. A `std::nullopt` value indicates that no context was found
  // for the given ID.
  using PageContextMap = std::map<
      std::string,
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>>;

  // Asynchronously fetches a single page context associated with the given
  // `webstate_unique_id`.
  void GetSingleContextAsync(
      const std::string& webstate_unique_id,
      base::OnceCallback<void(std::optional<std::unique_ptr<
                                  optimization_guide::proto::PageContext>>)>
          callback);

  // Asynchronously fetches multiple page contexts for the provided vector of
  // `webstate_unique_ids`.
  void GetMultipleContextsAsync(
      const std::vector<std::string>& webstate_unique_ids,
      base::OnceCallback<void(PageContextMap)> callback);

  // TabsDependencyInstaller
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

  // WebStateObserver:
  void WasHidden(web::WebState* web_state) override;

 private:
  friend class BrowserUserData<PersistTabContextBrowserAgent>;

  explicit PersistTabContextBrowserAgent(Browser* browser);

  // Called whenever scene activation level changed. This is explicitly used to
  // capture the event where a tab is hidden due to the app being backgrounded,
  // in which case the page context should be retrieved.
  void OnSceneActivationLevelChanged(SceneActivationLevel level);

  // Private callback for PageContextWrapper.
  void OnPageContextExtracted(const std::string& webstate_unique_id,
                              PageContextWrapperCallbackResponse response);

  // The service's PageContext wrapper.
  __strong PageContextWrapper* page_context_wrapper_;

  // Manages this object as an observer of `web_state_`.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // The service's PersistTabContextStateAgent. The state agent is used to
  // observe scene state changes such as the application being backgrounded, in
  // which case the last tab's page context should be saved to storage.
  __strong PersistTabContextStateAgent* persist_tab_context_state_agent_;

  // Profile-specific file path to store page contexts at in the app's cache.
  base::FilePath storage_directory_path_;

  // The sequenced task runner ensures that async tasks that are posted to it
  // execute in the same order as they were scheduled. This will be used to
  // ensure the creation of the user-specific directory is done before a write,
  // read or delete operation can be attempted. The task runner is created with
  // MayBlock, BEST_EFFORT task priority and SKIP_ON_SHUTDOWN task shutdown
  // behaviour.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<PersistTabContextBrowserAgent> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PERSIST_TAB_CONTEXT_BROWSER_AGENT_H_
