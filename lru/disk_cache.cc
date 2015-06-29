/*******************************************************************************
**          File: disk_cache.cc
**        Author: neevek <i@neevek.net>.
** Creation Time: 2014-12-25 Thu 06:04 PM
**   Description:  
*******************************************************************************/
#include "disk_cache.h"
#include "common/file_util.h"
#include "common/sha1/sha1.h"
#include "log/log.h"

namespace lru {

namespace {
  const std::string MAGIC_STRING("neevek_disklru");
  const std::string VERSION("1.0.0");
  const std::string JOURNAL_FILE("/journal");

  const char ACTION_READ = 'R'; // READ
  const char ACTION_UPDATE = 'U'; // UPDATE
  const char ACTION_DELETE = 'D'; // DELETE
  const char LINE_FEED = '\n';

  const int COMPACT_THRESHOLD = 2000;
  const float RETAIN_RATIO = 0.75f;

  std::string GenSha1Key(const std::string &key) {
    char sha1_buf[41];
    unsigned char sha1_hash[20];
    sha1::calc(key.c_str(), key.size(), sha1_hash);
    sha1::toHexString(sha1_hash, sha1_buf);
    return std::string(sha1_buf);
  }
};

DiskCache::DiskCache(const std::string &cache_dir, int app_version, 
  long max_cache_size, long max_item_count) :
  cache_dir_(cache_dir),
  app_version_(app_version), 
  max_item_count_(max_item_count),
  max_cache_size_(max_cache_size),
  cur_cache_size_(0),
  redundant_count_(0) {

  if (!FileUtil::DirExists(cache_dir)) {
    FileUtil::MakeDirs(cache_dir);
  }

  // start the background thread
  action_thread_ = std::thread(&DiskCache::RunQueuedActions, this);

  // run the INIT procedure in the background thread
  EnqueueAction(std::bind(&DiskCache::InitFromJournal, this));
}

DiskCache::~DiskCache() {
  action_queue_.QuitBlocking();
  action_thread_.join();
}

void DiskCache::InitFromJournal() {
  std::ifstream jn_ifstream;

  std::string jn_file(cache_dir_ + JOURNAL_FILE);
  std::string bak_jn_file(jn_file + ".bak");
  if (FileUtil::FileExists(bak_jn_file)) {
    std::rename(bak_jn_file.c_str(), jn_file.c_str());
  }

  jn_ifstream.open(jn_file, std::ios::binary);

  std::string line;
  if (FileUtil::FileExists(jn_file)) {
    if (!std::getline(jn_ifstream, line) || line != MAGIC_STRING || 
        !std::getline(jn_ifstream, line) || line != VERSION || 
        !std::getline(jn_ifstream, line) || line != std::to_string(app_version_) ||
        !std::getline(jn_ifstream, line) || line != "") {
      LOG_E("lru::DiskCache", "initializing from journal failed.");

      FileUtil::DeleteFile(jn_file);
      CompactJournalIfNeeded(false, true);
      return;
    }

  } else {
      CompactJournalIfNeeded(false, true);
      return;
  }

  LOG_V("lru::DiskCache", "journal file exists, ready to read it");

  ReadJournalFile(jn_file, jn_ifstream);
}

void DiskCache::ReadJournalFile(const std::string &jn_file, 
    std::ifstream &jn_ifstream) {

  std::string line;
  while (std::getline(jn_ifstream, line)) {
    if (line.size() == 0) {
      continue;
    }

    int first_space = line.find(' ');
    if (first_space == std::string::npos) {
      LOG_E("lru::DiskCache", "invalid line: %s", line.c_str());
      continue;
    }


    if (line[0] == ACTION_UPDATE) {
      int second_space = line.find(' ', first_space + 1);
      if (first_space == std::string::npos || 
          second_space == std::string::npos) {
        LOG_E("lru::DiskCache", "invalid line: %s", line.c_str());

        continue;
      }

      std::string sha1_key(line.substr(first_space + 1, 
            second_space - first_space - 1));

      LOG_V("lru::DiskCache", "reading line: %s", line.c_str());
      long file_size = std::stol(line.substr(second_space));
      HandleLineForUpdate(sha1_key, file_size);

    } else {
      std::string sha1_key(line.substr(first_space + 1));

      if (line[0] == ACTION_DELETE) {
        HandleLineForDelete(sha1_key);

      } else if (line[0] == ACTION_READ) {
        HandleLineForRead(sha1_key);
      }

    }
  }

  CompactJournalIfNeeded(false, false);

  journal_ofstream_.open(jn_file, std::ios::binary | std::ios::app);
  cond_.notify_all();

  LOG_V("lru::DiskCache", "LRU cached initialized. entry count=%zd, size=%ld", 
      entry_list_.size(), cur_cache_size_); 
}

void DiskCache::HandleLineForUpdate(const std::string &sha1_key, 
    long file_size) {

  auto iter = entry_map_.find(sha1_key);

  LOG_V("lru::Diskcache", "new=%d, new entry: %s, %ld", 
      iter == entry_map_.end(), sha1_key.c_str(), file_size);

  if (iter != entry_map_.end()) {
    // minus old file_size
    cur_cache_size_ -= iter->second->second;
    iter->second->second = file_size;

    entry_list_.splice(entry_list_.begin(), entry_list_, iter->second); 
    iter->second = entry_list_.begin();

    ++redundant_count_;

  } else {
    entry_list_.emplace_front(sha1_key, file_size);
    entry_map_.emplace(sha1_key, entry_list_.begin());
  }

  cur_cache_size_ += file_size;
}

void DiskCache::HandleLineForDelete(const std::string &sha1_key) {
  auto iter = entry_map_.find(sha1_key);
  if (iter != entry_map_.end()) {
    cur_cache_size_ -= iter->second->second;
    entry_map_.erase(iter);
    entry_list_.erase(iter->second); 
  }
  ++redundant_count_;
}

void DiskCache::HandleLineForRead(const std::string &sha1_key) {
  auto iter = entry_map_.find(sha1_key);
  if (iter != entry_map_.end()) {
    // move item to front
    entry_list_.splice(entry_list_.begin(), entry_list_, iter->second); 
    iter->second = entry_list_.begin();
  }
  ++redundant_count_;
}

bool DiskCache::Put(const std::string &key, WriteCacheDataFun &&fun) {
  if (key.size() == 0) {
    LOG_E("lru::DiskCache", "key is empty");
    return false;
  }

  std::string sha1_key = GenSha1Key(key);

  std::string dir(cache_dir_);
  dir.append(1, '/');
  dir.append(sha1_key.c_str(), 2);
  if (!FileUtil::DirExists(dir) && !FileUtil::MakeDirs(dir)) {
    LOG_E("lru::DiskCache", "failed to create dir: %s", dir.c_str());
    return false;
  }

  std::string file = GetCacheFile(sha1_key);

  // write cache data to a tmp file
  std::string tmp_file(file + ".tmp");
  auto data_ofstream = std::ofstream(tmp_file, std::ios::binary);
  if (!fun(data_ofstream)) {
    LOG_E("lru::DiskCache", "writing to file failed: %s", tmp_file.c_str());
    FileUtil::DeleteFile(tmp_file);
    return false;
  }
  long file_size = data_ofstream.tellp();
  data_ofstream.close();

  std::unique_lock<std::mutex> lock(mutex_);
  if (!IsInitialized()) {
    cond_.wait(lock);
  }

  // on success, rename the tmp file
  std::rename(tmp_file.c_str(), file.c_str());

  cur_cache_size_ += file_size;

  auto iter = entry_map_.find(sha1_key);
  if (iter != entry_map_.end()) {
    entry_list_.splice(entry_list_.begin(), entry_list_, iter->second); 
    iter->second = entry_list_.begin();

    cur_cache_size_ -= iter->second->second;
    iter->second->second = file_size;

  } else {
    entry_list_.emplace_front(sha1_key, file_size);
    entry_map_.emplace(sha1_key, entry_list_.begin());
  }

  LOG_V("lru::DiskCache", "entries: %ld, write file_size: %ld, %s=%ld", 
      entry_list_.size(), cur_cache_size_, sha1_key.c_str(), file_size);

  EnqueueAction([this, sha1_key, file_size, iter]{
    // write a log to the journal
    journal_ofstream_ << ACTION_UPDATE 
      << ' ' 
      << sha1_key 
      << ' ' 
      << file_size 
      << std::endl;

    if (iter != entry_map_.end()) {
      ++redundant_count_;
      //LOG_E("lru::DiskCache", "existing key: %s, redundant: %d", 
        //sha1_key.c_str(), redundant_count_);
    }

    EvictIfNeeded();
  });

  return true;
}

void DiskCache::EvictIfNeeded() {
  if (cur_cache_size_ > max_cache_size_ || 
      entry_list_.size() > max_item_count_) {

    LOG_D("lru::DiskCache", "start eviction, entries: %zd, size: %ld", 
        entry_list_.size(), cur_cache_size_);

    std::lock_guard<std::mutex> lock(mutex_);

    long target_size = max_cache_size_ * RETAIN_RATIO;
    long target_count = max_item_count_ * RETAIN_RATIO;

    LOG_V("lru::DiskCache", 
        "entries=%zd, cur_cache_size=%ld, going to remove...",
        entry_list_.size(), cur_cache_size_);

    while (cur_cache_size_ > target_size || 
        entry_list_.size() > target_count) {
      auto &item = entry_list_.back();    
      RemoveWithoutLocking(item.first, true);
    }

    LOG_D("lru::DiskCache", "after eviction, entries: %zd, size: %ld", 
        entry_list_.size(), cur_cache_size_);
  }
}

bool DiskCache::Get(const std::string &key, ReadCacheDataFun &&fun) {
  std::string sha1_key = GenSha1Key(key);

  std::ifstream fin;

  std::unique_lock<std::mutex> lock(mutex_);
  if (!IsInitialized()) {
    cond_.wait(lock);
  }

  auto iter = entry_map_.find(sha1_key);
  if (iter != entry_map_.end()) {
    std::string file = GetCacheFile(sha1_key);

    if (FileUtil::FileExists(file)) {
      // move item to front
      entry_list_.splice(entry_list_.begin(), entry_list_, iter->second); 
      iter->second = entry_list_.begin();

      lock.unlock();

      EnqueueAction([this, sha1_key]{
        // write a log to the journal
        journal_ofstream_ << ACTION_READ << ' ' << sha1_key << std::endl; 
        ++redundant_count_;

        CompactJournalIfNeeded(true, false);
      });

      fin.open(file, std::ios::binary);
      return fun(fin);

    } else {
      EnqueueAction([this, sha1_key]{
        RemoveWithLocking(sha1_key);
      });
    }

  }

  return false;
}

void DiskCache::Remove(const std::string &key) {
  std::string sha1_key = GenSha1Key(key);
  RemoveWithLocking(sha1_key);
}

bool DiskCache::RemoveWithLocking(const std::string &sha1_key) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!IsInitialized()) {
    cond_.wait(lock);
  }

  return RemoveWithoutLocking(sha1_key, false);
}

bool DiskCache::RemoveWithoutLocking(const std::string &sha1_key, 
    bool in_background) {

  auto iter = entry_map_.find(sha1_key);
  LOG_V("lru::DiskCache", ">>>>> removing... %s, %d", 
      sha1_key.c_str(), iter != entry_map_.end());

  if (iter != entry_map_.end()) {
    if (in_background) {
      DeleteCacheFileAndWriteJournal(sha1_key, iter);
    } else {
      EnqueueAction([this, sha1_key, iter]{
        DeleteCacheFileAndWriteJournal(sha1_key, iter);
      });
    }

    return true;
  }

  return false;
}

void DiskCache::DeleteCacheFileAndWriteJournal(const std::string &sha1_key, 
    EntryIterator iter) {

  // delete the cache file
  FileUtil::DeleteFile(GetCacheFile(sha1_key));

  // write a log to the journal
  journal_ofstream_ << ACTION_DELETE << ' ' << sha1_key << std::endl;
  ++redundant_count_;

  cur_cache_size_ -= iter->second->second;
  entry_map_.erase(iter);
  entry_list_.erase(iter->second); 

  CompactJournalIfNeeded(false, false);
}

void DiskCache::CompactJournalIfNeeded(bool should_lock, bool force) {
  if (!force && redundant_count_ < COMPACT_THRESHOLD) {
    return;
  }

  LOG_V("lru::DiskCache", "compact journal: %d, %d, %d", 
      force, redundant_count_, COMPACT_THRESHOLD);

  std::string jn_file(cache_dir_ + JOURNAL_FILE);
  std::string tmp_jn_file(jn_file + ".tmp");

  std::ofstream tmp_jn(tmp_jn_file, std::ios::binary);

  tmp_jn << MAGIC_STRING << LINE_FEED;
  tmp_jn << VERSION << LINE_FEED;
  tmp_jn << app_version_ << LINE_FEED;
  tmp_jn << LINE_FEED;

  std::unique_lock<std::mutex> lock;
  if (should_lock) {
    lock = std::unique_lock<std::mutex>(mutex_);
  }

  for (auto it = entry_list_.begin(); it != entry_list_.end(); ++it) {
    tmp_jn << ACTION_UPDATE << ' ';
    tmp_jn << it->first << ' ';
    tmp_jn << it->second << LINE_FEED;
  }
  tmp_jn.close();

  if (journal_ofstream_.is_open()) {
    journal_ofstream_.close();
    LOG_D("lru::DiskCache", "close original journal file");
  }

  std::string bak_jn_file(jn_file + ".bak");

  // rename original to bak
  if (FileUtil::FileExists(jn_file)) {
    FileUtil::DeleteFile(bak_jn_file);
    std::rename(jn_file.c_str(), bak_jn_file.c_str());

    LOG_D("lru::DiskCache", "backup original journal file");
  }

  // rename tmp to original 
  if (std::rename(tmp_jn_file.c_str(), jn_file.c_str()) == 0) {
    FileUtil::DeleteFile(bak_jn_file);

    LOG_D("lru::DiskCache", "rename tmp journal file to original journal file");
    LOG_D("lru::DiskCache", "%s -> %s", tmp_jn_file.c_str(), jn_file.c_str());
  }

  redundant_count_ = 0;

  journal_ofstream_.open(jn_file, std::ios::binary | std::ios::app);
  if (lock.owns_lock()) {
    lock.unlock();
  }
  cond_.notify_all();

  LOG_V("lru::DiskCache", "journal opened");
}

std::string DiskCache::GetCacheFile(const std::string &sha1_key) const {
  std::string file(cache_dir_);
  file.append(1, '/')
  .append(sha1_key.c_str(), 2)
  .append(1, '/')
  .append(sha1_key.c_str() + 2);

  return file;
}

void DiskCache::EnqueueAction(std::function<void()> &&action) {
  action_queue_.ForwardPushBack(std::forward<std::function<void()>>(action));
}

void DiskCache::RunQueuedActions() {
  while (action_queue_.HasNext()) {
    action_queue_.Front()();
    action_queue_.PopFront();
  }

  LOG_D("lru::DiskCache", "quit action queue.");
}

};  // namespace lru
