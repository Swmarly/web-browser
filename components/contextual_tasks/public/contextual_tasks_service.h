// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASKS_SERVICE_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASKS_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace contextual_tasks {

// Represents the eligibility status for contextual tasks features.
// This is used to determine if any backend is available and if the feature
// is enabled.
struct FeatureEligibility {
  // Whether the contextual tasks feature flag is enabled.
  bool contextual_tasks_enabled;
  // Whether the AIM backend is eligible for use.
  bool aim_eligible;

  bool IsEligible() const { return contextual_tasks_enabled && aim_eligible; }
};

// Service that allows clients to create and manage contextual tasks.
// See `ContextualTask` for more information on what a task is.
class ContextualTasksService : public KeyedService {
 public:
  // Whether a task was updated by  a change in the local or remote client.
  enum class TriggerSource {
    kUnown,
    kLocal,
    kRemote,
  };

  // Observers observing updates to the ContextualTask data which can be
  // originated by either the local or remote clients.
  class Observer : public base::CheckedObserver {
   public:
    // The service is about to be destroyed. Ensures observers have a chance to
    // remove references before service destruction.
    virtual void OnWillBeDestroyed() {}

    // The service is initialized and ready to take calls and return stored
    // tasks and threads.
    virtual void OnInitialized() {}

    // A new task was added at the given |source|.
    virtual void OnTaskAdded(const ContextualTask& task, TriggerSource source) {
    }

    // An existing task was updated at the given |source|.
    virtual void OnTaskUpdated(const ContextualTask& group,
                               TriggerSource source) {}

    // A task identifierd by `task_id` was removed.
    virtual void OnTaskRemoved(const base::Uuid& task_id,
                               TriggerSource source) {}
  };

  ContextualTasksService();
  ~ContextualTasksService() override;

  // Returns whether there are any available backends that are eligible for use.
  virtual FeatureEligibility GetFeatureEligibility() = 0;

  // Whether service is initialized.
  virtual bool IsInitialized() = 0;

  // Methods for creating and managing tasks.
  virtual ContextualTask CreateTask() = 0;
  virtual ContextualTask CreateTaskFromUrl(const GURL& url) = 0;
  virtual void GetTaskById(
      const base::Uuid& task_id,
      base::OnceCallback<void(std::optional<ContextualTask>)> callback)
      const = 0;
  virtual void GetTasks(
      base::OnceCallback<void(std::vector<ContextualTask>)> callback) const = 0;
  virtual void DeleteTask(const base::Uuid& task_id) = 0;

  // Methods related to server-side conversations.
  // When assigning a thread to a task_id that does not have a registered
  // task, the ContextualTask is created on the fly. We do not automatically
  // create tasks when removing threads.
  virtual void AddThreadToTask(const base::Uuid& task_id,
                               const Thread& thread) = 0;
  virtual void RemoveThreadFromTask(const base::Uuid& task_id,
                                    ThreadType type,
                                    const std::string& server_id) = 0;
  virtual void UpdateThreadTurnId(const base::Uuid& task_id,
                                  ThreadType thread_type,
                                  const std::string& server_id,
                                  const std::string& conversation_turn_id) = 0;

  // Methods related to attaching URLs to tasks.
  virtual void AttachUrlToTask(const base::Uuid& task_id, const GURL& url) = 0;
  virtual void DetachUrlFromTask(const base::Uuid& task_id,
                                 const GURL& url) = 0;

  // Gets the context for a given task. The `context_callback` will receive the
  // a contextual task. If the `sources` set is empty, all available sources
  // will be used. The callback will be invoked with the enriched context, or
  // `nullptr` if the task is not found.
  virtual void GetContextForTask(
      const base::Uuid& task_id,
      const std::set<ContextualTaskContextSource>& sources,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback) = 0;

  // Methods related to associating tabs to tasks using their tab ID.
  virtual void AssociateTabWithTask(const base::Uuid& task_id,
                                    SessionID tab_id) = 0;
  virtual void DisassociateTabFromTask(const base::Uuid& task_id,
                                       SessionID tab_id) = 0;
  virtual std::optional<ContextualTask> GetContextualTaskForTab(
      SessionID tab_id) const = 0;
  virtual void ClearAllTabAssociationsForTask(const base::Uuid& task_id) = 0;

  // Add / remove observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  // Returns DataTypeControllerDelegate for the contextual task thread datatype.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetAiThreadControllerDelegate() = 0;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASKS_SERVICE_H_
