// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_TAB_STRIP_COLLECTION_H_
#define COMPONENTS_TABS_PUBLIC_TAB_STRIP_COLLECTION_H_

#include <memory>
#include <optional>
#include <set>
#include <unordered_map>

#include "base/types/pass_key.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/tab_collection.h"

class TabStripModel;

namespace tabs {

class TabInterface;
class UnpinnedTabCollection;
class PinnedTabCollection;
class TabGroupTabCollection;
class SplitTabCollection;

using NodeMovePosition = std::tuple<ChildPtr, size_t>;

// TabStripCollection is the storage representation of a tabstrip
// in a browser. This contains a pinned collection and an unpinned
// collection which then contain different tabs and group.
class TabStripCollection : public TabCollection {
 public:
  TabStripCollection();
  ~TabStripCollection() override;
  TabStripCollection(const TabStripCollection&) = delete;
  TabStripCollection& operator=(const TabStripCollection&) = delete;

  PinnedTabCollection* pinned_collection() { return pinned_collection_; }
  UnpinnedTabCollection* unpinned_collection() { return unpinned_collection_; }

  size_t IndexOfFirstNonPinnedTab() const;

  void AddTabRecursive(std::unique_ptr<TabInterface> tab,
                       size_t index,
                       std::optional<tab_groups::TabGroupId> new_group_id,
                       bool new_pinned_state);

  void MoveTabRecursive(size_t initial_index,
                        size_t final_index,
                        std::optional<tab_groups::TabGroupId> new_group_id,
                        bool new_pinned_state);
  void MoveTabsRecursive(
      const std::vector<int>& tab_indices,
      size_t destination_index,
      std::optional<tab_groups::TabGroupId> new_group_id,
      bool new_pinned_state,
      const std::set<TabCollection::Type>& retain_collection_types);

  // Removes the tab present at a recursive index in the collection and
  // returns the unique_ptr to the tab model. If there is no tab present
  // due to bad input then CHECK.
  std::unique_ptr<TabInterface> RemoveTabAtIndexRecursive(size_t index);

  // TabCollection:
  // Tabs and Collections are not allowed to be removed from TabStripCollection.
  // `MaybeRemoveTab` and `MaybeRemoveCollection` will return nullptr.
  std::unique_ptr<TabInterface> MaybeRemoveTab(TabInterface* tab) override;
  std::unique_ptr<TabCollection> MaybeRemoveCollection(
      TabCollection* collection) override;

  void InsertTabCollectionAt(
      std::unique_ptr<TabCollection> collection,
      int index,
      int pinned,
      std::optional<tab_groups::TabGroupId> parent_group);

  // Remove a tab collection and send the appropriate notifications.
  std::unique_ptr<TabCollection> RemoveTabCollection(TabCollection* collection);

  // Adds the `tab_group_collection` to `detached_group_collections_`
  // so that it can be used when inserting a tab to a group.
  void CreateTabGroup(
      std::unique_ptr<tabs::TabGroupTabCollection> tab_group_collection);

  // Group operations.
  // NOTE: These operations only work for attached tab groups.

  TabGroupTabCollection* GetTabGroupCollection(tab_groups::TabGroupId group_id);
  // Returns a list of all tab group IDs, the order of the IDs is not
  // guaranteed.
  std::vector<tab_groups::TabGroupId> GetAllTabGroupIds() const;

  // Detached tab group operations.

  // Clears the detached group with `group_id` in `detached_group_collections_`.
  // Crashes if the group is not found in the detached tab groups list.
  void CloseDetachedTabGroup(const tab_groups::TabGroupId& group_id);
  // Returns the detached tab group with `group_id` if it exists, returns
  // nullptr otherwise.
  TabGroupTabCollection* GetDetachedTabGroup(
      const tab_groups::TabGroupId& group_id);

  // Split tab operations.
  SplitTabCollection* GetSplitTabCollection(split_tabs::SplitTabId split_id);
  void CreateSplit(split_tabs::SplitTabId split_id,
                   const std::vector<TabInterface*>& tabs,
                   split_tabs::SplitTabVisualData visual_data);
  void Unsplit(split_tabs::SplitTabId split_id);
  void ValidateData() const;

  // Helper method to calculate a valid sequence of moves when a bunch of tabs
  // and collections are being moved to a `destination_index`.
  std::vector<NodeMovePosition> CalculateIncrementalChildMoves(
      ChildrenPtrs tab_or_collections,
      size_t destination_index);

  std::optional<const tab_groups::TabGroupId> FindGroupIdFor(
      const tabs::TabCollection::Handle& collection_handle,
      base::PassKey<TabStripModel>) const;

 private:
  // Adds a tab to a particular recursive index in the collection.
  void AddTabRecursiveImpl(std::unique_ptr<TabInterface> tab,
                           size_t index,
                           std::optional<tab_groups::TabGroupId> new_group_id,
                           bool new_pinned_state);

  // Removes the tab from the collection. If `close_empty_group_collection` is
  // true then group collection is closed when the last tab is removed from
  // the group collection.
  std::unique_ptr<TabInterface> RemoveTabRecursiveImpl(
      TabInterface* tab,
      bool close_empty_group_collection = true);

  // Removes the group collection with `group_id` from
  // `detached_group_collections_`.
  std::unique_ptr<tabs::TabGroupTabCollection> PopDetachedGroupCollection(
      const tab_groups::TabGroupId& group_id);

  void CreateGroupCollectionForMove(const ChildPtr& tab_or_collection,
                                    size_t final_index,
                                    tab_groups::TabGroupId new_group_id);

  void MoveTabOrCollectionRecursive(
      ChildPtr tab_or_collection,
      size_t final_index,
      std::optional<tab_groups::TabGroupId> new_group_id,
      bool new_pinned_state);

  // Returns the list of tabs and collection to remove for `MoveTabsRecursive`.
  // `retain_collection_types` adds the fully selected collections based on the
  // types passed in and adds the collection to be moved instead of the tabs
  // in the collection.
  ChildrenPtrs GetTabsAndCollectionsForMove(
      const std::vector<int>& tab_indices,
      const std::set<TabCollection::Type>& retain_collection_types);

  // Helper to centralize updates to `group_mapping_` and `split_mapping_`. If
  // `root_collection` is a group, the appropriate splits need to group need to
  // be updated in the `split_mapping_`.
  void AddCollectionMapping(TabCollection* root_collection);
  void RemoveCollectionMapping(TabCollection* root_collection);

  void AddTabImpl(std::unique_ptr<TabInterface> tab,
                  const TabCollection::Position& position);
  std::unique_ptr<TabInterface> RemoveTabImpl(TabInterface* tab);
  void AddTabCollectionImpl(std::unique_ptr<TabCollection> collection,
                            const TabCollection::Position& position);
  std::unique_ptr<TabCollection> RemoveTabCollectionImpl(
      TabCollection* collection);
  void MoveTabImpl(TabInterface* tab_ptr,
                   const TabCollection::Position& position);
  void MoveCollectionImpl(TabCollection* collection_ptr,
                          const TabCollection::Position& position);
  void MoveTabImpl(TabInterface* tab_ptr,
                   size_t final_index,
                   std::optional<tab_groups::TabGroupId> new_group_id,
                   bool new_pinned_state);
  void MoveCollectionImpl(TabCollection* collection_ptr,
                          size_t final_index,
                          std::optional<tab_groups::TabGroupId> new_group_id,
                          bool new_pinned_state);

  // Helper to compute the parent collection and direct index in the collection
  // to insert a tab or collection based on insertion properties like the
  // recursive index, pinned state and group to insert.
  TabCollection::Position GetInsertionDetails(
      int index,
      int pinned,
      std::optional<tab_groups::TabGroupId> group);

  // All of the pinned tabs for this tabstrip is present in this collection.
  // This should be below `impl_` to avoid being a dangling pointer during
  // destruction.
  raw_ptr<PinnedTabCollection> pinned_collection_ = nullptr;

  // All of the unpined tabs and groups for this tabstrip is present in this
  // collection. This should be below `impl_` to avoid being a dangling pointer
  // during destruction.
  raw_ptr<UnpinnedTabCollection> unpinned_collection_ = nullptr;

  // Lookup table to find group collections by their group ID.
  std::unordered_map<tab_groups::TabGroupId,
                     raw_ptr<TabGroupTabCollection>,
                     tab_groups::TabGroupIdHash>
      group_mapping_;

  // Lookup table to find split collections by their split ID.
  std::unordered_map<split_tabs::SplitTabId,
                     raw_ptr<SplitTabCollection>,
                     split_tabs::SplitTabIdHash>
      split_mapping_;

  // `tab_strip_model` creates this to allow extension of lifetime for groups to
  // allow for group_model_ updates and observation methods.
  std::vector<std::unique_ptr<tabs::TabGroupTabCollection>>
      detached_group_collections_;
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_TAB_STRIP_COLLECTION_H_
