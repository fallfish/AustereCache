#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__
#include <vector>
#include <thread>
#include <mutex>
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
}


#endif
