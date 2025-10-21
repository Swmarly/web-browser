// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_FAKE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_FAKE_TOOL_REQUEST_H_

#include "chrome/browser/actor/tools/tool_request.h"

namespace actor {

// A fake tool request that creates a FakeTool.
class FakeToolRequest : public ToolRequest {
 public:
  FakeToolRequest(base::OnceClosure on_invoke, base::OnceClosure on_destroy);
  ~FakeToolRequest() override;

  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;

  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string JournalEvent() const override;

 private:
  mutable base::OnceClosure on_invoke_;
  mutable base::OnceClosure on_destroy_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_FAKE_TOOL_REQUEST_H_
