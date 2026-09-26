// :bustub-keep-private:
//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// arc_replacer.cpp
//
// Identification: src/buffer/arc_replacer.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/arc_replacer.h"
#include <algorithm>
#include <memory>
#include <optional>
#include "common/config.h"
#include "common/macros.h"

namespace bustub {

/**
 *
 * TODO(P1): Add implementation
 *
 * @brief a new ArcReplacer, with lists initialized to be empty and target size to 0
 * @param num_frames the maximum number of frames the ArcReplacer will be required to cache
 */
ArcReplacer::ArcReplacer(size_t num_frames) : replacer_size_(num_frames) {}

/**
 * TODO(P1): Add implementation
 *
 * @brief Performs the Replace operation as described by the writeup
 * that evicts from either mfu_ or mru_ into its corresponding ghost list
 * according to balancing policy.
 *
 * If you wish to refer to the original ARC paper, please note that there are
 * two changes in our implementation:
 * 1. When the size of mru_ equals the target size, we don't check
 * the last access as the paper did when deciding which list to evict from.
 * This is fine since the original decision is stated to be arbitrary.
 * 2. Entries that are not evictable are skipped. If all entries from the desired side
 * (mru_ / mfu_) are pinned, we instead try victimize the other side (mfu_ / mru_),
 * and move it to its corresponding ghost list (mfu_ghost_ / mru_ghost_).
 *
 * @return frame id of the evicted frame, or std::nullopt if cannot evict
 */
auto ArcReplacer::Evict() -> std::optional<frame_id_t> {
    std::lock_guard<std::mutex> lock(latch_);
    if (mru_.size() >= mru_target_size_) {
        for (auto it = mru_.end(); it != mru_.begin();) {
            --it;
            auto fid = *it;
            auto alive_item = alive_map_.at(fid);
            if (!alive_item->evictable_) {
                continue;
            }
            mru_.erase(alive_item->live_iter_);
            alive_item->arc_status_ = ArcStatus::MRU_GHOST;
            mru_ghost_.push_front(alive_item->page_id_);
            alive_item->ghost_iter_ = mru_ghost_.begin();
            auto alive_node = alive_map_.extract(fid);
            alive_node.key() = alive_item->page_id_;
            ghost_map_.insert(std::move(alive_node));
            curr_size_--;
            return fid;
        }
        for (auto it = mfu_.end(); it != mfu_.begin();) {
            --it;
            auto fid = *it;
            auto alive_item = alive_map_.at(fid);
            if (!alive_item->evictable_) {
                continue;
            }
            mfu_.erase(alive_item->live_iter_);
            alive_item->arc_status_ = ArcStatus::MFU_GHOST;
            mfu_ghost_.push_front(alive_item->page_id_);
            alive_item->ghost_iter_ = mfu_ghost_.begin();
            auto alive_node = alive_map_.extract(fid);
            alive_node.key() = alive_item->page_id_;
            ghost_map_.insert(std::move(alive_node));
            curr_size_--;
            return fid;
        }
    } else {
        for (auto it = mfu_.end(); it != mfu_.begin();) {
            --it;
            auto fid = *it;
            auto alive_item = alive_map_.at(fid);
            if (!alive_item->evictable_) {
                continue;
            }
            mfu_.erase(alive_item->live_iter_);
            alive_item->arc_status_ = ArcStatus::MFU_GHOST;
            mfu_ghost_.push_front(alive_item->page_id_);
            alive_item->ghost_iter_ = mfu_ghost_.begin();
            auto alive_node = alive_map_.extract(fid);
            alive_node.key() = alive_item->page_id_;
            ghost_map_.insert(std::move(alive_node));
            curr_size_--;
            return fid;
        }
        for (auto it = mru_.end(); it != mru_.begin();) {
            --it;
            auto fid = *it;
            auto alive_item = alive_map_.at(fid);
            if (!alive_item->evictable_) {
                continue;
            }
            mru_.erase(alive_item->live_iter_);
            alive_item->arc_status_ = ArcStatus::MRU_GHOST;
            mru_ghost_.push_front(alive_item->page_id_);
            alive_item->ghost_iter_ = mru_ghost_.begin();
            auto alive_node = alive_map_.extract(fid);
            alive_node.key() = alive_item->page_id_;
            ghost_map_.insert(std::move(alive_node));
            curr_size_--;
            return fid;
        }
    }
    return std::nullopt;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Record access to a frame, adjusting ARC bookkeeping accordingly
 * by bring the accessed page to the front of mfu_ if it exists in any of the lists
 * or the front of mru_ if it does not.
 *
 * Performs the operations EXCEPT REPLACE described in original paper, which is
 * handled by `Evict()`.
 *
 * Consider the following four cases, handle accordingly:
 * 1. Access hits mru_ or mfu_
 * 2/3. Access hits mru_ghost_ / mfu_ghost_
 * 4. Access misses all the lists
 *
 * This routine performs all changes to the four lists as preperation
 * for `Evict()` to simply find and evict a victim into ghost lists.
 *
 * Note that frame_id is used as identifier for alive pages and
 * page_id is used as identifier for the ghost pages, since page_id is
 * the unique identifier to the page after it's dead.
 * Using page_id for alive pages should be the same since it's one to one mapping,
 * but using frame_id is slightly more intuitive.
 *
 * @param frame_id id of frame that received a new access.
 * @param page_id id of page that is mapped to the frame.
 * @param access_type type of access that was received. This parameter is only needed for
 * leaderboard tests.
 */
void ArcReplacer::RecordAccess(frame_id_t frame_id, page_id_t page_id, [[maybe_unused]] AccessType access_type) {
    std::lock_guard<std::mutex> lock(latch_);
    auto alive_it = alive_map_.find(frame_id);
    if (alive_it != alive_map_.end()){
        auto alive_item = alive_it->second;
        if (alive_item->arc_status_ == ArcStatus::MRU) {
            mru_.erase(alive_item->live_iter_);
        } else {
            mfu_.erase(alive_item->live_iter_);
        }
        alive_item->arc_status_ = ArcStatus::MFU;
        mfu_.push_front(frame_id);
        alive_item->live_iter_ = mfu_.begin();
    }else{
        auto ghost_node = ghost_map_.extract(page_id);
        if (!ghost_node.empty()){
            ghost_node.key() = frame_id;
            auto& ghost_item = ghost_node.mapped();
            if (ghost_item->arc_status_ == ArcStatus::MRU_GHOST) {
                if (mru_ghost_.size() >= mfu_ghost_.size()) {
                    mru_target_size_ = std::min(replacer_size_, mru_target_size_ + 1);
                } else {
                    mru_target_size_ = std::min(replacer_size_, mru_target_size_ + mfu_ghost_.size() / mru_ghost_.size());
                }
                mru_ghost_.erase(ghost_item->ghost_iter_);
            } else {
                if (mfu_ghost_.size() >= mru_ghost_.size()) {
                    mru_target_size_ = (mru_target_size_ == 0) ? 0 : mru_target_size_ - 1;
                } else {
                    size_t delta = mru_ghost_.size() / mfu_ghost_.size();
                    mru_target_size_ = (mru_target_size_ < delta) ? 0 : mru_target_size_ - delta;
                }
                mfu_ghost_.erase(ghost_item->ghost_iter_);
            }
            ghost_item->arc_status_ = ArcStatus::MFU;
            ghost_item->frame_id_ = frame_id;
            ghost_item->evictable_ = false;
            mfu_.push_front(frame_id);
            ghost_item->live_iter_ = mfu_.begin();
            alive_map_.insert(std::move(ghost_node));
        }else{
            if (mru_.size() + mru_ghost_.size() == replacer_size_) {
                if (!mru_ghost_.empty()) {
                    ghost_map_.erase(mru_ghost_.back());
                    mru_ghost_.pop_back();
                }
            } else if (mru_.size() + mru_ghost_.size() + mfu_.size() + mfu_ghost_.size() == 2 * replacer_size_) {
                if (!mfu_ghost_.empty()) {
                    ghost_map_.erase(mfu_ghost_.back());
                    mfu_ghost_.pop_back();
                }
            }
            auto alive_item = std::make_shared<FrameStatus>(page_id, frame_id, false, ArcStatus::MRU);
            mru_.push_front(frame_id);
            alive_item->live_iter_ = mru_.begin();
            alive_map_.emplace(frame_id, std::move(alive_item));
        }
    }
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Toggle whether a frame is evictable or non-evictable. This function also
 * controls replacer's size. Note that size is equal to number of evictable entries.
 *
 * If a frame was previously evictable and is to be set to non-evictable, then size should
 * decrement. If a frame was previously non-evictable and is to be set to evictable,
 * then size should increment.
 *
 * If frame id is invalid, throw an exception or abort the process.
 *
 * For other scenarios, this function should terminate without modifying anything.
 *
 * @param frame_id id of frame whose 'evictable' status will be modified
 * @param set_evictable whether the given frame is evictable or not
 */
void ArcReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
    std::lock_guard<std::mutex> lock(latch_);
    auto it = alive_map_.find(frame_id);
    auto &status = it->second;
    if (status->evictable_ == set_evictable) {
        return;
    }
    status->evictable_ = set_evictable;
    if (set_evictable) {
        curr_size_++;
    } else {
        curr_size_--;
    }
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Remove an evictable frame from replacer.
 * This function should also decrement replacer's size if removal is successful.
 *
 * Note that this is different from evicting a frame, which always remove the frame
 * decided by the ARC algorithm.
 *
 * If Remove is called on a non-evictable frame, throw an exception or abort the
 * process.
 *
 * If specified frame is not found, directly return from this function.
 *
 * @param frame_id id of frame to be removed
 */
void ArcReplacer::Remove(frame_id_t frame_id) {
    std::lock_guard<std::mutex> lock(latch_);
    auto it = alive_map_.find(frame_id);
    if (it == alive_map_.end()) {
        return;
    }
    auto status = it->second;
    if (status->arc_status_ == ArcStatus::MRU) {
        mru_.erase(status->live_iter_);
    } else {
        mfu_.erase(status->live_iter_);
    }
    alive_map_.erase(it);
    curr_size_--;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Return replacer's size, which tracks the number of evictable frames.
 *
 * @return size_t
 */
auto ArcReplacer::Size() -> size_t {
    std::lock_guard<std::mutex> lock(latch_);
    return curr_size_;
}

}  // namespace bustub
