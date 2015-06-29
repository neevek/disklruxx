/*******************************************************************************
**          File: memory_cache.h
**        Author: neevek <i@neevek.net>.
** Creation Time: 2015-06-28 Sun 11:08 PM
**   Description:  
*******************************************************************************/
#ifndef MEMORY_CACHE_H_
#define MEMORY_CACHE_H_
#include <string>
#include <map>
#include <list>
#include <mutex>
#include <condition_variable>

namespace lru {

class MemoryCache {
 public:
   using SizeCalculator = std::function<size_t(const std::string &key, void *value)>;
   using EvictionHandler = std::function<void(const std::string &key, void *value)>;

 public:
   MemoryCache(long max_cache_size, 
       long max_item_count, 
       SizeCalculator size_calculator,
       EvictionHandler eviction_handler);
   ~MemoryCache() = default;

 public:
   void *Get(const std::string &key);
   // return the old value if exists
   void Put(const std::string &key, void *value);
   void Remove(const std::string &key);
   inline long ItemCount() const;
   inline long MaxItemCount() const;
   inline long CurrentCacheSize() const;
   inline long MaxCacheSize() const;

 private:
   void EvictIfNeeded();
   void RemoveInternal(const std::string &key);

 private:
   using ListElement = std::pair<std::string, void *>;
   using EntryIterator = std::map<std::string, std::list<ListElement>::iterator>::iterator;
   std::map<std::string, std::list<ListElement>::iterator> entry_map_;
   std::list<ListElement> entry_list_;

 private:
   long max_cache_size_;
   long max_item_count_;
   long cur_cache_size_;
   SizeCalculator calculate_obj_size;
   EvictionHandler on_obj_evicted;

   std::mutex mutex_;
   std::condition_variable cond_;
};  // class MemoryCache

long MemoryCache::ItemCount() const {
  return entry_list_.size();
}

long MemoryCache::MaxItemCount() const {
  return max_item_count_;
}

long MemoryCache::CurrentCacheSize() const {
  return cur_cache_size_;
}

long MemoryCache::MaxCacheSize() const {
  return max_cache_size_;
}

};  // namespace lru

#endif /* end of include guard: MEMORY_CACHE_H_ */
