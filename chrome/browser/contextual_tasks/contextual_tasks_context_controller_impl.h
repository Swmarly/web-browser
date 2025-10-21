// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_IMPL_H_

#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace contextual_tasks {
class ContextualTasksService;
enum class ContextualTaskContextSource;

}  // namespace contextual_tasks

namespace contextual_tasks {

class ContextualTasksContextControllerImpl
    : public ContextualTasksContextController {
 public:
  explicit ContextualTasksContextControllerImpl(
      ContextualTasksService* service);
  ~ContextualTasksContextControllerImpl() override;

  // ContextualTasksService implementation.
  FeatureEligibility GetFeatureEligibility() override;
  bool IsInitialized() override;
  ContextualTask CreateTask() override;
  ContextualTask CreateTaskFromUrl(const GURL& url) override;
  void GetTaskById(const base::Uuid& task_id,
                   base::OnceCallback<void(std::optional<ContextualTask>)>
                       callback) const override;
  void GetTasks(base::OnceCallback<void(std::vector<ContextualTask>)> callback)
      const override;
  void DeleteTask(const base::Uuid& task_id) override;
  void AddThreadToTask(const base::Uuid& task_id,
                       const Thread& thread) override;
  void RemoveThreadFromTask(const base::Uuid& task_id,
                            ThreadType type,
                            const std::string& server_id) override;
  void UpdateThreadTurnId(const base::Uuid& task_id,
                          ThreadType thread_type,
                          const std::string& server_id,
                          const std::string& conversation_turn_id) override;
  void AttachUrlToTask(const base::Uuid& task_id, const GURL& url) override;
  void DetachUrlFromTask(const base::Uuid& task_id, const GURL& url) override;
  void GetContextForTask(
      const base::Uuid& task_id,
      const std::set<ContextualTaskContextSource>& sources,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback) override;
  void AssociateTabWithTask(const base::Uuid& task_id,
                            SessionID tab_id) override;
  void DisassociateTabFromTask(const base::Uuid& task_id,
                               SessionID tab_id) override;
  std::optional<ContextualTask> GetContextualTaskForTab(
      SessionID tab_id) const override;
  void ClearAllTabAssociationsForTask(const base::Uuid& task_id) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetAiThreadControllerDelegate() override;

 private:
  raw_ptr<ContextualTasksService> service_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_IMPL_H_
