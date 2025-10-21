// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_CONTEXT_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_CONTEXT_H_

#include <string>
#include <vector>

#include "base/uuid.h"
#include "components/sessions/core/session_id.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace contextual_tasks {

// Enum representing the different sources that can contribute to the context of
// a contextual task.
enum class ContextualTaskContextSource {
  kFallbackTitle,
  kFaviconService,
  kHistoryService,
  kTabStrip,
};

class ContextualTask;

// Data block for UrlAttachment, intended to be modified only by
// ContextDecorator implementations.
struct UrlAttachmentDecoratorData {
  UrlAttachmentDecoratorData();
  ~UrlAttachmentDecoratorData();
  UrlAttachmentDecoratorData(const UrlAttachmentDecoratorData&);
  UrlAttachmentDecoratorData& operator=(const UrlAttachmentDecoratorData&);
  UrlAttachmentDecoratorData(UrlAttachmentDecoratorData&&);
  UrlAttachmentDecoratorData& operator=(UrlAttachmentDecoratorData&&);

  // Filled in by ContextualTaskContextSource::kFallbackTitle.
  struct FallbackTitleData {
    std::u16string title;
  };
  FallbackTitleData fallback_title_data;

  // Filled in by ContextualTaskContextSource::kFaviconService.
  struct FaviconData {
    gfx::Image image;
    GURL icon_url;
  };
  FaviconData favicon_data;

  // Filled in by ContextualTaskContextSource::kHistoryService.
  struct HistoryData {
    std::u16string title;
  };
  HistoryData history_data;

  // Filled in by ContextualTaskContextSource::kTabStrip.
  struct TabStripData {
    std::u16string title;
    bool is_open_in_tab_strip = false;
  };
  TabStripData tab_strip_data;
};

// Represents a URL that is attached to a `ContextualTask`. This struct contains
// the URL itself and a data block that can be populated by decorators.
struct UrlAttachment {
 public:
  explicit UrlAttachment(const GURL& url);
  ~UrlAttachment();

  // Accessor methods.
  GURL GetURL() const;
  std::u16string GetTitle() const;
  gfx::Image GetFavicon() const;
  bool IsOpen() const;

  // Gives access to internal data sources.
  UrlAttachmentDecoratorData& GetMutableDecoratorDataForTesting();

 private:
  friend class ContextDecorator;

  // ContextDecorator implementation can access this method through a protected
  // method.
  UrlAttachmentDecoratorData& GetMutableDecoratorData();

  // The URL that is attached.
  GURL url_;

  // A data block that can be populated by decorators with additional metadata
  // about the URL.
  UrlAttachmentDecoratorData decorator_data_;
};

// Represents the context associated with a `ContextualTask`. This is a
// snapshot of the context at a given point in time and is not kept in sync
// with the `ContextualTask`. It is passed through a chain of decorators to be
// enriched with additional metadata.
struct ContextualTaskContext {
 public:
  // Constructs a `ContextualTaskContext` from a `ContextualTask`.
  explicit ContextualTaskContext(const ContextualTask& task);
  ~ContextualTaskContext();

  ContextualTaskContext(const ContextualTaskContext& other);
  ContextualTaskContext(ContextualTaskContext&& other);
  ContextualTaskContext& operator=(const ContextualTaskContext& other);
  ContextualTaskContext& operator=(ContextualTaskContext&& other);

  // Returns the unique ID of the task this context is for.
  const base::Uuid& GetTaskId() const;

  // Returns the URL attachments for the task.
  const std::vector<UrlAttachment>& GetUrlAttachments() const;

  // Returns a mutable version of the URL attachments for the task.
  std::vector<UrlAttachment>& GetMutableUrlAttachmentsForTesting();

 private:
  friend class ContextDecorator;

  // Returns a mutable version of the URL attachments for the task.
  std::vector<UrlAttachment>& GetMutableUrlAttachments();

  // The unique ID of the task this context is for.
  base::Uuid task_id_;

  // The URL attachments for the task.
  std::vector<UrlAttachment> urls_;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_CONTEXTUAL_TASK_CONTEXT_H_
