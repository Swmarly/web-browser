// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"

#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/string_number_conversions.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/metrics/persist_tab_context_metrics.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/paths/paths_internal.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

// TODO(crbug.com/445963646): Add metrics logging to browser agent for cleanup
// functions and for tracking the difference between the number of web states vs
// the number of page contexts in storage.
// TODO(crbug.com/447646545): Add test coverage for persist tab context

namespace {

// Prefix used for filenames of persisted page context files.
constexpr std::string kPageContextPrefix = "page_context_";

// Suffix used for filenames of persisted page context files.
constexpr std::string kProtoSuffix = ".proto";

// The name of the subdirectory within the user's cache directory where
// persisted tab contexts are stored.
constexpr std::string kPersistedTabContexts = "persisted_tab_contexts";

// The delay applied before the PurgeExpiredPageContexts task is executed
// after the PersistTabContextBrowserAgent is initialized.
constexpr base::TimeDelta kPurgeTaskDelay = base::Seconds(3);

// Constructs the full file path for a given webstate_unique_id within the
// storage directory.
base::FilePath GetContextFilePath(const base::FilePath& storage_directory_path,
                                  const std::string& webstate_unique_id) {
  std::string filename = kPageContextPrefix + webstate_unique_id + kProtoSuffix;
  return storage_directory_path.Append(filename);
}

// Writes serialized page contexts containing web state URLs, titles, APCs and
// inner text to a user-specific storage path located in the app's cache.
void WriteContextToStorage(std::string_view serialized_page_context,
                           const std::string& webstate_unique_id,
                           base::FilePath storage_directory_path) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (storage_directory_path.empty()) {
    base::UmaHistogramEnumeration(
        kWriteTabContextResultHistogram,
        IOSPersistTabContextWriteResult::kStoragePathEmptyFailure);
    return;
  }

  base::FilePath file_path =
      GetContextFilePath(storage_directory_path, webstate_unique_id);

  if (!base::WriteFile(file_path, serialized_page_context)) {
    base::UmaHistogramEnumeration(
        kWriteTabContextResultHistogram,
        IOSPersistTabContextWriteResult::kWriteFailure);
    return;
  }

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes(kPersistTabContextWriteTimeHistogram, elapsed_time);

  base::UmaHistogramEnumeration(kWriteTabContextResultHistogram,
                                IOSPersistTabContextWriteResult::kSuccess);

  base::UmaHistogramCounts10M(kPersistTabContextSizeHistogram,
                              serialized_page_context.size());
}

void DeleteContextFromStorage(const std::string& webstate_unique_id,
                              base::FilePath storage_directory_path) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (storage_directory_path.empty()) {
    base::UmaHistogramEnumeration(
        kDeleteTabContextResultHistogram,
        IOSPersistTabContextDeleteResult::kStoragePathEmptyFailure);
    return;
  }

  base::FilePath file_path =
      GetContextFilePath(storage_directory_path, webstate_unique_id);

  if (!base::DeleteFile(file_path)) {
    base::UmaHistogramEnumeration(
        kDeleteTabContextResultHistogram,
        IOSPersistTabContextDeleteResult::kDeleteFailure);
    return;
  }

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes(kPersistTabContextDeleteTimeHistogram, elapsed_time);

  base::UmaHistogramEnumeration(kDeleteTabContextResultHistogram,
                                IOSPersistTabContextDeleteResult::kSuccess);
}

// Creates the profile-specific directory in which serialized page contexts will
// be stored. This function is scheduled using the sequenced task runner to
// ensure it is created before any attempts are made to write, read or delete
// to/from the directory while improving performance by only creating it once.
void CreateStorageDirectory(const base::FilePath& storage_directory_path) {
  if (base::CreateDirectory(storage_directory_path)) {
    base::UmaHistogramBoolean(kCreateDirectoryResultHistogram, true);
  } else {
    // Log the failure to UMA.
    base::UmaHistogramBoolean(kCreateDirectoryResultHistogram, false);
  }
}

// Reads the serialized proto data from the file. Returns std::nullopt on
// failure.
std::optional<std::string> ReadFileContents(
    const base::FilePath& storage_directory_path,
    const std::string& unique_id) {
  if (storage_directory_path.empty()) {
    base::UmaHistogramEnumeration(
        kReadTabContextResultHistogram,
        IOSPersistTabContextReadResult::kStoragePathEmptyFailure);
    return std::nullopt;
  }

  base::FilePath file_path =
      GetContextFilePath(storage_directory_path, unique_id);

  if (!base::PathExists(file_path)) {
    base::UmaHistogramEnumeration(
        kReadTabContextResultHistogram,
        IOSPersistTabContextReadResult::kFileNotFound);
    return std::nullopt;
  }

  std::string serialized_data;
  if (!base::ReadFileToString(file_path, &serialized_data)) {
    base::UmaHistogramEnumeration(kReadTabContextResultHistogram,
                                  IOSPersistTabContextReadResult::kReadFailure);
    return std::nullopt;
  }

  return serialized_data;
}

// Parses serialized data into a PageContext proto. Returns std::nullopt on
// failure.
std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>
ParsePageContext(const std::string& serialized_data) {
  auto page_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  if (!page_context->ParseFromString(serialized_data)) {
    base::UmaHistogramEnumeration(
        kReadTabContextResultHistogram,
        IOSPersistTabContextReadResult::kParseFailure);
    return std::nullopt;
  }

  return page_context;
}

// Reads and parses a page context from persistent storage using the given
// `webstate_unique_id`. The context is expected to be stored in a file named
// "page_context_<unique_id>.proto" within the profile-specific cache
// directory.
std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>
ReadAndParseContextFromStorage(const base::FilePath& storage_directory_path,
                               const std::string& webstate_unique_id) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  std::optional<std::string> serialized_data =
      ReadFileContents(storage_directory_path, webstate_unique_id);
  if (!serialized_data) {
    DeleteContextFromStorage(webstate_unique_id, storage_directory_path);
    return std::nullopt;
  }

  std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>
      page_context = ParsePageContext(*serialized_data);
  if (!page_context) {
    DeleteContextFromStorage(webstate_unique_id, storage_directory_path);
    return std::nullopt;
  }

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes(kPersistTabContextReadTimeHistogram, elapsed_time);

  base::UmaHistogramEnumeration(kReadTabContextResultHistogram,
                                IOSPersistTabContextReadResult::kSuccess);

  return page_context;
}

// Performs multiple ReadAndParseContextFromStorage calls.
PersistTabContextBrowserAgent::PageContextMap DoMultipleContextReads(
    const base::FilePath& storage_directory_path,
    const std::vector<std::string>& webstate_unique_ids) {
  PersistTabContextBrowserAgent::PageContextMap result_map;
  for (const std::string& unique_id : webstate_unique_ids) {
    result_map[unique_id] =
        ReadAndParseContextFromStorage(storage_directory_path, unique_id);
  }

  return result_map;
}

// Deletes page context proto files from `contexts_dir` whose last modified time
// is older than the set time to live.
void PurgeExpiredPageContexts(base::FilePath contexts_dir,
                              base::TimeDelta ttl) {
  if (!base::DirectoryExists(contexts_dir)) {
    return;
  }

  base::Time now = base::Time::Now();
  base::FileEnumerator enumerator(
      contexts_dir, /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat({kPageContextPrefix, "*", kProtoSuffix}));
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    base::Time last_modified = info.GetLastModifiedTime();
    base::TimeDelta age = now - last_modified;

    if (age >= ttl) {
      if (!base::DeleteFile(name)) {
        // TODO(crbug.com/445963646): Add metrics logging to browser agent for
        // cleanup functions.
      } else {
        // TODO(crbug.com/445963646): Add metrics logging to browser agent for
        // cleanup functions.
      }
    }
  }
}

// Recursively deletes the entire persisted contexts directory.
void DeletePersistedContextsDirectory(base::FilePath contexts_dir) {
  if (!base::DirectoryExists(contexts_dir)) {
    // TODO(crbug.com/445963646): Add metrics logging to browser agent for
    // cleanup functions.
    return;
  }

  // TODO(crbug.com/445963646): Add metrics logging to browser agent for
  // cleanup functions.
  base::DeletePathRecursively(contexts_dir);
}

}  // namespace

#pragma mark - Public

PersistTabContextBrowserAgent::PersistTabContextBrowserAgent(Browser* browser)
    : BrowserUserData(browser),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  ProfileIOS* profile = browser->GetProfile();
  CHECK(profile);
  base::FilePath cache_directory_path;
  ios::GetUserCacheDirectory(profile->GetStatePath(), &cache_directory_path);
  storage_directory_path_ = cache_directory_path.Append(kPersistedTabContexts);

  if (IsPersistTabContextEnabled()) {
    task_runner_->PostTask(FROM_HERE, base::BindOnce(&CreateStorageDirectory,
                                                     storage_directory_path_));

    persist_tab_context_state_agent_ = [[PersistTabContextStateAgent alloc]
        initWithTransitionCallback:
            base::BindRepeating(
                &PersistTabContextBrowserAgent::OnSceneActivationLevelChanged,
                weak_factory_.GetWeakPtr())];
    [browser->GetSceneState() addObserver:persist_tab_context_state_agent_];
    StartObserving(browser, Policy::kAccordingToFeature);

    PrefService* prefs = profile->GetPrefs();
    CHECK(prefs);
    base::TimeDelta ttl = GetPersistedContextEffectiveTTL(prefs);
    // Schedule a cleanup task with a delay.
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PurgeExpiredPageContexts, storage_directory_path_, ttl),
        kPurgeTaskDelay);
  } else {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DeletePersistedContextsDirectory,
                       storage_directory_path_),
        kPurgeTaskDelay);
  }
}

PersistTabContextBrowserAgent::~PersistTabContextBrowserAgent() {
  if (persist_tab_context_state_agent_) {
    StopObserving();
    [browser_->GetSceneState() removeObserver:persist_tab_context_state_agent_];
  }
  page_context_wrapper_ = nil;
  web_state_observation_.Reset();
}

void PersistTabContextBrowserAgent::GetSingleContextAsync(
    const std::string& webstate_unique_id,
    base::OnceCallback<void(
        std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>)>
        callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadAndParseContextFromStorage, storage_directory_path_,
                     webstate_unique_id),
      std::move(callback));
}

void PersistTabContextBrowserAgent::GetMultipleContextsAsync(
    const std::vector<std::string>& webstate_unique_ids,
    base::OnceCallback<void(PageContextMap)> callback) {
  if (webstate_unique_ids.empty()) {
    std::move(callback).Run({});
    return;
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DoMultipleContextReads, storage_directory_path_,
                     webstate_unique_ids),
      base::BindOnce(std::move(callback)));
}

void PersistTabContextBrowserAgent::OnWebStateInserted(
    web::WebState* web_state) {
  // Nothing to do.
}

void PersistTabContextBrowserAgent::OnWebStateRemoved(
    web::WebState* web_state) {
  // Nothing to do.
}

void PersistTabContextBrowserAgent::OnWebStateDeleted(
    web::WebState* web_state) {
  if (!web_state) {
    return;
  }

  std::string webstate_unique_id =
      base::NumberToString(web_state->GetUniqueIdentifier().identifier());

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeleteContextFromStorage, webstate_unique_id,
                                storage_directory_path_));
}

void PersistTabContextBrowserAgent::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {
  web_state_observation_.Reset();
  if (new_active) {
    web_state_observation_.Observe(new_active);
  }
}

#pragma mark - WebStateObserver

void PersistTabContextBrowserAgent::WasHidden(web::WebState* web_state) {
  if (!web_state) {
    return;
  }

  // Cancel any ongoing page context operation.
  if (page_context_wrapper_) {
    page_context_wrapper_ = nil;
  }

  std::string webstate_unique_id =
      base::NumberToString(web_state->GetUniqueIdentifier().identifier());

  page_context_wrapper_ = [[PageContextWrapper alloc]
        initWithWebState:web_state
      completionCallback:
          base::BindOnce(&PersistTabContextBrowserAgent::OnPageContextExtracted,
                         weak_factory_.GetWeakPtr(), webstate_unique_id)];
  [page_context_wrapper_ setShouldGetAnnotatedPageContent:YES];
  [page_context_wrapper_ setShouldGetInnerText:YES];
  [page_context_wrapper_ setIsLowPriorityExtraction:YES];
  [page_context_wrapper_ populatePageContextFieldsAsync];
}

#pragma mark - Private

void PersistTabContextBrowserAgent::OnSceneActivationLevelChanged(
    SceneActivationLevel level) {
  if (level != SceneActivationLevelBackground) {
    return;
  }

  web::WebState* active_web_state = web_state_observation_.GetSource();

  if (active_web_state) {
    WasHidden(active_web_state);
  }
}

void PersistTabContextBrowserAgent::OnPageContextExtracted(
    const std::string& webstate_unique_id,
    PageContextWrapperCallbackResponse response) {
  if (!response.has_value()) {
    return;
  }

  std::string serialized_page_context;
  response.value()->SerializeToString(&serialized_page_context);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WriteContextToStorage, std::move(serialized_page_context),
                     webstate_unique_id, storage_directory_path_));
}
