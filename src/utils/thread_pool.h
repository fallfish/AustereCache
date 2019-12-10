#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__
#include <vector>
#include <thread>
#include <mutex>

#include <condition_variable>
#include <queue>
#include <functional>
#include <chrono>

namespace cache {
// A "nice and neat" threadpool that reuses thread objects from StackOverflow
// https://stackoverflow.com/a/29742586
class AThreadPool
{
  public:

  AThreadPool (int threads) : shutdown_ (false)
  {
    // Create the specified number of threads
    threads_.reserve (threads);
    for (int i = 0; i < threads; ++i)
      threads_.emplace_back (std::bind (&AThreadPool::threadEntry, this, i));
  }

  ~AThreadPool ()
  {
    {
      // Unblock any threads and tell them to stop
      std::unique_lock <std::mutex> l (lock_);

      shutdown_ = true;
      condVar_.notify_all();
    }

    // Wait for all threads to stop
    //std::cerr << "Joining threads" << std::endl;
    for (auto& thread : threads_)
      thread.join();
  }

  void doJob (std::function <void (void)> func)
  {
    // Place a job on the queue and unblock a thread
    {
      std::unique_lock <std::mutex> l (lock_);

      jobs_.emplace (std::move (func));
      condVar_.notify_one();
      while (jobs_.size() >= 2 * threads_.size()) {
        condVarMaster_.wait(l);
      }
    }
  }

  protected:

  void threadEntry (int i)
  {
    std::function <void (void)> job;

    while (1)
    {
      {
        std::unique_lock <std::mutex> l (lock_);

        while (! shutdown_ && jobs_.empty()) {
          condVar_.wait (l);
        }

        if (jobs_.empty ())
        {
          // No jobs to do and we are shutting down
          //std::cerr << "Thread " << i << " terminates" << std::endl;
          return;
        }

        job = std::move (jobs_.front ());
        jobs_.pop();
      }

      // Do the job without holding any locks
      job ();
      if (jobs_.size() < threads_.size()) {
        condVarMaster_.notify_one();
      }
    }

  }

  std::mutex lock_;
  std::condition_variable condVar_, condVarMaster_;
  bool shutdown_;
  std::queue <std::function <void (void)>> jobs_;
  std::vector <std::thread> threads_;
};
}


#endif
