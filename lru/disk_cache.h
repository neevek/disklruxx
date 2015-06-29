/*******************************************************************************
**          File: disk_cache.h
**        Author: neevek <i@neevek.net>.
** Creation Time: 2014-12-25 Thu 11:06 AM
**   Description:  
*******************************************************************************/
#ifndef DISK_CACHE_H_
#define DISK_CACHE_H_
#include <string>
#include <fstream>
#include <map>
#include <list>
#include <functional>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "common/blocking_queue.h"

namespace lru {

class DiskCache {
 public:
   using WriteCacheDataFun = std::function<bool(std::ofstream &)>;
   using ReadCacheDataFun = std::function<bool(std::ifstream &)>;

   DiskCache(const std::string &cache_dir, int app_version, 
       long max_cache_size, long max_item_count);
   ~DiskCache();

 public:
   bool Put(const std::string &key, WriteCacheDataFun &&fun);
   bool Get(const std::string &key, ReadCacheDataFun &&fun);
   void Remove(const std::string &key);
   inline bool IsInitialized() const;
   inline long ItemCount() const;
   inline long MaxItemCount() const;
   inline long CurrentCacheSize() const;
   inline long MaxCacheSize() const;

 private:
   using ListElement = std::pair<std::string, long>;
   using EntryIterator = std::map<std::string, std::list<ListElement>::iterator>::iterator;
   std::map<std::string, std::list<ListElement>::iterator> entry_map_;
   std::list<ListElement> entry_list_;
   
 private:
   void InitFromJournal();
   void ReadJournalFile(const std::string &jn_file, std::ifstream &jn_ifstream);
   void HandleLineForUpdate(const std::string &sha1_key, long file_size);
   void HandleLineForDelete(const std::string &sha1_key);
   void HandleLineForRead(const std::string &sha1_key);

   void EvictIfNeeded();
   void CompactJournalIfNeeded(bool should_lock, bool force);
   std::string GetCacheFile(const std::string &sha1_key) const;
   void EnqueueAction(std::function<void()> &&action);

   bool RemoveWithLocking(const std::string &sha1_key);
   bool RemoveWithoutLocking(const std::string &sha1_key, bool in_background);
   void DeleteCacheFileAndWriteJournal(const std::string &sha1_key, 
       EntryIterator iter);

   void RunQueuedActions();

 private:
   std::string cache_dir_;
   long app_version_;
   long max_item_count_;
   long max_cache_size_;
   long cur_cache_size_;
   int redundant_count_;

   std::ofstream journal_ofstream_;
   BlockingQueue<std::function<void()>> action_queue_;

   std::thread action_thread_;
   std::mutex mutex_;
   std::condition_variable cond_;
};

bool DiskCache::IsInitialized() const {
  return journal_ofstream_.is_open();
}

long DiskCache::ItemCount() const {
  return entry_list_.size();
}

long DiskCache::MaxItemCount() const {
  return max_item_count_;
}

long DiskCache::CurrentCacheSize() const {
  return cur_cache_size_;
}

long DiskCache::MaxCacheSize() const {
  return max_cache_size_;
}

};  // namespace lru

#endif /* end of include guard: DISK_CACHE_H_ */

