#include "lru/memory_cache.h"
#include "log/log.h"
#include <thread>
#include <iostream>

int main(int argc, const char *argv[]) {
  lru::MemoryCache cache(1024*5, 3, 
      [](const std::string &key, void *value){ 
        std::string *s = reinterpret_cast<std::string *>(value);
        return s->size() * 1; 
      }, 
      [](const std::string &key, void *value){
        std::string *s = reinterpret_cast<std::string *>(value);
        std::cout << "evicted: " << *s << std::endl; 
        delete s;
      });


  cache.Put("a", new std::string("aaaaaaaaa"));
  cache.Put("b", new std::string("bbbbbbbbb"));
  cache.Put("c", new std::string("ccccccccc"));
  std::string *s2 = (std::string *)cache.Get("a");
  cache.Put("d", new std::string("ddddddddd"));

  std::string *s = (std::string *)cache.Get("a");
  if (s) {
    std::cout << "found: '" << *s << "' for key: b" << std::endl;
  } else {
    std::cout << "not found for key " << "b" << std::endl;
  }
  std::cout << "item count: " << cache.ItemCount() << std::endl;
  std::cout << "cache size: " << cache.CurrentCacheSize() << std::endl;

  cache.Remove("a");

  cache.EvictAll();

  std::cout << "item count: " << cache.ItemCount() << std::endl;
  std::cout << "cache size: " << cache.CurrentCacheSize() << std::endl;
  
  return 0;
}
