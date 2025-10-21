// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_CHOOSER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_CHOOSER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/task/task_runner.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {

class WebContents;

// This is a ui::SelectFileDialog::Listener implementation that grants access to
// the selected files to a specific renderer process on success, and then calls
// a callback on a specific task runner. Furthermore the listener will delete
// itself when any of its listener methods are called.
// All of this class has to be called on the UI thread.
class CONTENT_EXPORT FileSystemChooser : public ui::SelectFileDialog::Listener,
                                         WebContentsObserver {
 public:
  using ResultCallback =
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr,
                              std::vector<content::PathInfo>)>;

  class CONTENT_EXPORT Options {
   public:
    Options(ui::SelectFileDialog::Type type,
            blink::mojom::AcceptsTypesInfoPtr accepts_types_info,
            std::u16string title,
            base::FilePath default_directory,
            base::FilePath suggested_name);
    Options(const Options&);
    ~Options();

    ui::SelectFileDialog::Type type() const { return type_; }
    const ui::SelectFileDialog::FileTypeInfo& file_type_info() const {
      return file_types_;
    }
#if BUILDFLAG(IS_ANDROID)
    const std::vector<std::u16string>& mime_types() const {
      return mime_types_;
    }
#endif
    const std::u16string& title() const { return title_; }
    const base::FilePath& default_path() const { return default_path_; }
    int default_file_type_index() const { return default_file_type_index_; }

   private:
    base::FilePath ResolveSuggestedNameExtension(
        base::FilePath suggested_name,
        ui::SelectFileDialog::FileTypeInfo& file_types);

    ui::SelectFileDialog::Type type_;
    ui::SelectFileDialog::FileTypeInfo file_types_;
    int default_file_type_index_ = 0;
#if BUILDFLAG(IS_ANDROID)
    std::vector<std::u16string> mime_types_;
#endif
    std::u16string title_;
    // Combination of optional default_directory and optional suggested_name.
    // Wiill end with a trailing separator if suggested_name is empty.
    base::FilePath default_path_;
  };

  // Struct to hold objects that should be kept alive for the lifetime of the
  // chooser.
  struct CONTENT_EXPORT ScopedObjects {
    ScopedObjects();
    ~ScopedObjects();
    ScopedObjects(ScopedObjects&&);
    ScopedObjects& operator=(ScopedObjects&&);
    ScopedObjects(const ScopedObjects&) = delete;
    ScopedObjects& operator=(const ScopedObjects&) = delete;
    ScopedObjects(base::ScopedClosureRunner&& fullscreen_block,
                  base::ScopedClosureRunner&& pip_tucker);

    base::ScopedClosureRunner fullscreen_block;
    base::ScopedClosureRunner pip_tucker;
  };

  static void CreateAndShow(WebContents* web_contents,
                            const Options& options,
                            ResultCallback callback,
                            ScopedObjects scoped_objects);

  // Returns whether the specified extension receives special handling by the
  // Windows shell. These extensions should be sanitized before being shown in
  // the "save as" file picker.
  static bool IsShellIntegratedExtension(
      const base::FilePath::StringType& extension);

  FileSystemChooser(ui::SelectFileDialog::Type type,
                    ResultCallback callback,
                    ScopedObjects scoped_objects,
                    WebContents* web_contents);

 private:
  ~FileSystemChooser() override;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override;
  void FileSelectionCanceled() override;

  // WebContentsObserver
  void OnVisibilityChanged(Visibility visibility) override;
  SEQUENCE_CHECKER(sequence_checker_);

  const ui::SelectFileDialog::Type type_;
  ResultCallback callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  ScopedObjects scoped_objects_ GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<ui::SelectFileDialog> dialog_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_CHOOSER_H_
