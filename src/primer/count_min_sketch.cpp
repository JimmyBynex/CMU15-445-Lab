//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// count_min_sketch.cpp
//
// Identification: src/primer/count_min_sketch.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "primer/count_min_sketch.h"
#include <sys/types.h>

#include <stdexcept>
#include <string>

namespace bustub {

/**
 * Constructor for the count-min sketch.
 *
 * @param width The width of the sketch matrix.
 * @param depth The depth of the sketch matrix.
 * @throws std::invalid_argument if width or depth are zero.
 */
template <typename KeyType>
CountMinSketch<KeyType>::CountMinSketch(uint32_t width, uint32_t depth) : width_(width), depth_(depth) {
  if (width == 0 || depth == 0) {
    throw std::invalid_argument("CountMinSketch width and depth must be non-zero.");
  }

  /** @spring2026 PLEASE DO NOT MODIFY THE FOLLOWING */
  // Initialize seeded hash functions
  hash_functions_.reserve(depth_);
  for (size_t i = 0; i < depth_; i++) {
    hash_functions_.push_back(this->HashFunction(i));
  }
  table_ = std::vector<std::atomic_uint32_t>(static_cast<size_t>(width_) * depth_);
}

template <typename KeyType>
CountMinSketch<KeyType>::CountMinSketch(CountMinSketch &&other) noexcept
    : width_(other.width_), depth_(other.depth_), table_(std::move(other.table_)) {
  hash_functions_.reserve(depth_);
  for (size_t i = 0; i < depth_; i++) {
    hash_functions_.push_back(this->HashFunction(i));
  }
  other.width_ = 0;
  other.depth_ = 0;
  other.hash_functions_.clear();
}

template <typename KeyType>
auto CountMinSketch<KeyType>::operator=(CountMinSketch &&other) noexcept -> CountMinSketch & {
  if (this == &other) {
    return *this;
  }
  width_ = other.width_;
  depth_ = other.depth_;
  hash_functions_.clear();
  hash_functions_.reserve(depth_);
  for (size_t i = 0; i < depth_; i++) {
    hash_functions_.push_back(this->HashFunction(i));
  }
  table_ = std::move(other.table_);
  other.width_ = 0;
  other.depth_ = 0;
  other.hash_functions_.clear();
  return *this;
}

template <typename KeyType>
void CountMinSketch<KeyType>::Insert(const KeyType &item) {
  /** @TODO(student) Implement this function! */
  size_t row = 0;
  for (auto &hash_func : hash_functions_) {
    size_t col = hash_func(item) % width_;
    table_[row * width_ + col].fetch_add(1, std::memory_order_relaxed);
    row++;
  }
}

template <typename KeyType>
void CountMinSketch<KeyType>::Merge(const CountMinSketch<KeyType> &other) {
  if (width_ != other.width_ || depth_ != other.depth_) {
    throw std::invalid_argument("Incompatible CountMinSketch dimensions for merge.");
  }
  /** @TODO(student) Implement this function! */
  for (size_t i = 0; i < depth_; ++i) {
    for (size_t j = 0; j < width_; ++j) {
      size_t index = i * width_ + j;
      table_[index].fetch_add(other.table_[index].load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
  }
}

template <typename KeyType>
auto CountMinSketch<KeyType>::Count(const KeyType &item) const -> uint32_t {
  uint32_t min_count = UINT32_MAX;
  for (size_t i =0;i<depth_;i++){
    size_t j = hash_functions_[i](item);
    size_t index = i * width_ + j;
    uint32_t count = table_[index].load(std::memory_order_relaxed);
    if (count < min_count) {
      min_count = count;
    }
  }
  return min_count;
}

template <typename KeyType>
void CountMinSketch<KeyType>::Clear() {
  /** @TODO(student) Implement this function! */
  for (auto &count : table_) {
    count.store(0, std::memory_order_relaxed);
  }
}

template <typename KeyType>
auto CountMinSketch<KeyType>::TopK(uint16_t k, const std::vector<KeyType> &candidates)
    -> std::vector<std::pair<KeyType, uint32_t>> {
  /** @TODO(student) Implement this function! */
  auto ku = std::vector<std::pair<KeyType, uint32_t>>();
  for (const auto &candidate : candidates) {
    uint32_t count = Count(candidate);
    ku.push_back(std::make_pair(candidate, count));
  }
  std::sort(ku.begin(), ku.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
  if (ku.size() > k) {
    ku.resize(k);
  }
  return ku;
}

// Explicit instantiations for all types used in tests
template class CountMinSketch<std::string>;
template class CountMinSketch<int64_t>;  // For int64_t tests
template class CountMinSketch<int>;      // This covers both int and int32_t
}  // namespace bustub
