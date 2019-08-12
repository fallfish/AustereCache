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

/**
 * @brief Support multi-threading for each chunk read / write
 */
class ThreadPool {
 public:
  ThreadPool(int num_threads) :
      _num_threads(num_threads) {
    _threads.resize(_num_threads);
    _free_threads = std::make_unique<bool[]>(_num_threads);
    for (int i = 0; i < _num_threads; i++)
      _free_threads[i] = true;
  }

  /**
   * @brief Get a free thread from the thread pool
   *
   * @param th if success, store the thread pointer into th
   *
   * @return 
   */
  int allocate_thread()
  {
    std::lock_guard<std::mutex> lock(_mutex);
    for (int i = 0; i < _num_threads; i++) {
      if (_free_threads[i]) {
        _free_threads[i] = false;
        return i;
      }
    }
    return -1;
  }

  void return_thread(int index)
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _free_threads[index] = true;
  }

  std::shared_ptr<std::thread>& get_thread(int index)
  {
    return _threads[index];
  }

  void wait_and_return_threads(std::vector<int> &threads)
  {
    for (int thread_id : threads) {
      get_thread(thread_id)->join();
    }
    for (int thread_id: threads) {
      get_thread(thread_id).reset();
      return_thread(thread_id);
    }
    threads.clear();
  }

 private:
  std::vector<std::shared_ptr<std::thread>> _threads;
  std::unique_ptr< bool[] > _free_threads;
  int _num_threads;
  std::mutex _mutex;
};


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
    // Place a job on the queu and unblock a thread
    std::unique_lock <std::mutex> l (lock_);

    jobs_.emplace (std::move (func));
    condVar_.notify_one();
  }

  protected:

  void threadEntry (int i)
  {
    std::function <void (void)> job;

    while (1)
    {
      {
        std::unique_lock <std::mutex> l (lock_);

        while (! shutdown_ && jobs_.empty())
          condVar_.wait (l);

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
    }

  }

  std::mutex lock_;
  std::condition_variable condVar_;
  bool shutdown_;
  std::queue <std::function <void (void)>> jobs_;
  std::vector <std::thread> threads_;
};
}


#endif
