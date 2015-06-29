/*******************************************************************************
**          File: memory_cache.cc
**        Author: neevek <i@neevek.net>.
** Creation Time: 2015-06-29 Mon 10:19 AM
**   Description:  
*******************************************************************************/
#include "memory_cache.h"
#include "log/log.h"

namespace lru {

  namespace {
    const float RETAIN_RATIO = 0.75f;
  };

  MemoryCache::MemoryCache(long max_cache_size, 
      long max_item_count, 
      SizeCalculator size_calculator,
      EvictionHandler eviction_handler) : 
    max_cache_size_(max_cache_size),
    max_item_count_(max_item_count), 
    cur_cache_size_(0),
    calculate_obj_size(size_calculator),
    on_obj_evicted(eviction_handler) {

}
void *MemoryCache::Get(const std::string &key) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto iter = entry_map_.find(key);
  if (iter != entry_map_.end()) {
    // move item to front
    entry_list_.splice(entry_list_.begin(), entry_list_, iter->second); 
    iter->second = entry_list_.begin();

    return iter->second->second;
  }

  return nullptr;
}

void MemoryCache::Put(const std::string &key, void *value) {
  std::lock_guard<std::mutex> lock(mutex_);

  void *old_value = nullptr;

  auto iter = entry_map_.find(key);
  if (iter != entry_map_.end()) {
    old_value = iter->second->second;
    cur_cache_size_ -= calculate_obj_size(key, old_value);
    on_obj_evicted(key, old_value);

    entry_list_.splice(entry_list_.begin(), entry_list_, iter->second); 
    iter->second = entry_list_.begin();
    iter->second->second = value;

    LOG_V("lru::MemoryCache", "replaced the old key: %s", key.c_str());

  } else {
    entry_list_.emplace_front(key, value);
    entry_map_.emplace(key, entry_list_.begin());
  }

  cur_cache_size_ += calculate_obj_size(key, value);

  EvictIfNeeded();
}

void MemoryCache::Remove(const std::string &key) {
  std::lock_guard<std::mutex> lock(mutex_);
  RemoveInternal(key);
}

void MemoryCache::EvictIfNeeded() {
  if (cur_cache_size_ > max_cache_size_ || 
      entry_list_.size() > max_item_count_) {

    LOG_D("lru::MemoryCache", "start eviction, entries: %zd, size: %zd", 
        entry_list_.size(), cur_cache_size_);

    long target_size = max_cache_size_ * RETAIN_RATIO;
    long target_count = max_item_count_ * RETAIN_RATIO;

    while (cur_cache_size_ > target_size || 
        entry_list_.size() > target_count) {

      auto &item = entry_list_.back();    
      RemoveInternal(item.first);
    }

    LOG_D("lru::MemoryCache", "after eviction, entries: %zd, size: %ld", 
        entry_list_.size(), cur_cache_size_);
  }
}

void MemoryCache::RemoveInternal(const std::string &key) {
  auto iter = entry_map_.find(key);
  if (iter != entry_map_.end()) {
    void *value = iter->second->second;

    cur_cache_size_ -= calculate_obj_size(key, value);
    on_obj_evicted(key, value);

    entry_list_.erase(iter->second);
    entry_map_.erase(iter);
  }
}

}; // namespace lru
