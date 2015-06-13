/*******************************************************************************
**          File: blocking_queue.h
**        Author: neevek <i@neevek.net>.
** Creation Time: 2014-10-13 Mon 09:28 AM
**   Description: A deque wrapper that offers *blocking* test
*******************************************************************************/
#ifndef BLOCKING_QUEUE_H_
#define BLOCKING_QUEUE_H_
#include <deque>
#include <mutex>
#include <condition_variable>

template <typename T>
class BlockingQueue {
 public:
   BlockingQueue() { }

   void PushBack(T obj) {
     std::unique_lock<std::mutex> lock(mutex_);

     deque_.push_back(obj);

     lock.unlock();
     cond_.notify_one();
   }

   void PushFront(T obj) {
     std::unique_lock<std::mutex> lock(mutex_);

     deque_.push_front(obj);

     lock.unlock();
     cond_.notify_one();
   }

   void ForwardPushBack(T &&obj) {
     std::unique_lock<std::mutex> lock(mutex_);

     deque_.push_back(std::forward<T>(obj));

     lock.unlock();
     cond_.notify_one();
   }

   void ForwardPushFront(T &&obj) {
     std::unique_lock<std::mutex> lock(mutex_);

     deque_.push_front(std::forward<T>(obj));

     lock.unlock();
     cond_.notify_one();
   }

   template <class ...Args>
   void EmplaceBack(Args &&...args) {
     std::unique_lock<std::mutex> lock(mutex_);

     deque_.emplace_back(args...);

     lock.unlock();
     cond_.notify_one();
   }

   template <class ...Args>
   void EmplaceFront(Args &&...args) {
     std::unique_lock<std::mutex> lock(mutex_);

     deque_.emplace_front(args...);

     lock.unlock();
     cond_.notify_one();
   }

   // this method will block if the queue is empty
   bool HasNext(int wait_time = -1) {
     std::unique_lock<std::mutex> lock(mutex_);
     running_ = true;
     if (deque_.size() == 0 && wait_time != 0) {
       if (wait_time > 0) {
         cond_.wait_for(lock, std::chrono::milliseconds(wait_time));
       } else {
         cond_.wait(lock, [this]{ return !running_ || !deque_.empty(); });
       }
     }

     return deque_.size() > 0;
   }

   const T &Front() const {
     std::unique_lock<std::mutex> lock(mutex_);
     const T &obj = deque_.front();
     return obj;
   }

   T &Front() {
     std::unique_lock<std::mutex> lock(mutex_);
     return deque_.front();
   }

   void PopFront() {
     std::unique_lock<std::mutex> lock(mutex_);
     deque_.pop_front();
   }

   uint32_t Size() const {
     return deque_.size();
   }

   bool Empty() const {
     return deque_.empty();
   }

   void Clear() {
     deque_.clear();
   }

   void QuitBlocking() {
     std::unique_lock<std::mutex> lock(mutex_);
     running_ = false;
     lock.unlock();
     cond_.notify_one();
   }

   const std::deque<T> &GetDeque() const {
     return deque_;
   }

 private:
   std::deque<T> deque_;
   std::mutex mutex_;
   std::condition_variable cond_;

   bool running_;
};

#endif /* end of include guard: BLOCKING_QUEUE_H_ */
