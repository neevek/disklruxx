#include "lru/disk_cache.h"
#include "log/log.h"
#include <thread>

void test_read_write_with_multithreads(lru::DiskCache &cache) {
  const int tc = 10;
  LOG_V("main", "start testing WRITE and READ with 10 threads...");

  std::thread threads[tc];
  for (int i = 0; i < tc; ++i) {
    threads[i] = std::thread([&cache]{
      std::srand(std::time(0));

      for (int i = 0; i < 3000; ++i) {
        int value = std::rand() % 10000;

        if (std::rand() % 10000 < 5000) {
          cache.Put(std::to_string(value), [value](std::ofstream &of) {
            of << value; 
              LOG_V("main", "WRITE data: %d", value);
              return true;
            });

        } else {
          bool found = cache.Get(std::to_string(value), [value](std::ifstream &fin) {
            std::string data;
            int buf_size = 1024;
            char buf[buf_size + 1];

            while(fin.good()) {
              fin.read(buf, buf_size);
              int count = fin.gcount();
              if (count > 0) {
                data.append(buf, count);
              } else {
                break;
              }
            }

            LOG_V("main", "READ data: %s", data.c_str());

            return true;
          });

          LOG_V("main", "read cache with get: %d", found);
        }
      }
    });
  }

  LOG_D("main", "start to join thread");
  for (int i = 0; i < tc; ++i) {
    threads[i].join();
  }

  LOG_D("main", "all threads exit.");
  LOG_D("main", "cache_size=%ld, item_count=%ld", 
      cache.CurrentCacheSize(), cache.ItemCount());
}

int main(int argc, const char *argv[]) {
  lru::DiskCache cache("path/to/cache", 100, 10240, 1000);
  test_read_write_with_multithreads(cache);

  printf("\nExecute the following commands to check the result:\n");
  printf("find path/to/cache -type f | fgrep -v journal | xargs ls -l | awk '{a+=$5}END{print a, NR}'\n");
  printf("awk '/U/{a[$2]=$3; next} /D/{delete a[$2]}END{for(e in a){c+=a[e]} print c, length(a)}' path/to/cache/journal\n\n");
  
  std::chrono::milliseconds dura(300);
  std::this_thread::sleep_for(dura);

  return 0;
}
