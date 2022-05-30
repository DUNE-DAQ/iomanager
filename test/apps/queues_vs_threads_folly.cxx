#include "iomanager/CommonIssues.hpp"
#include "iomanager/FollyQueue.hpp"
#include <chrono>
#include <iostream>

#include "iomanager/ConnectionId.hpp"
#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"

#include "folly/concurrency/DynamicBoundedQueue.h"

using namespace std::chrono;

// n_got here is an attempt to stop the optimizer turning things into no-ops
void pop_thread(int n, int64_t& t, int& n_got)
{
  folly::DMPMCQueue<int, true> q(1000);

  int N = 5000;
  milliseconds timeout(0);
  auto start = steady_clock::now();

  for(int i=0; i<N; ++i){
    int x;
    if(q.try_dequeue_for(x, timeout)) {
      ++n_got;
    }
  }
  auto end = steady_clock::now();
  t=duration_cast<nanoseconds>(end - start).count()/N;
}

double get_mean_pop_time(int n_threads)
{
  std::vector<std::thread> threads;
  std::vector<int64_t> ts(n_threads);
  std::vector<int> n_got(n_threads);
  int n_got_total=0;
  for(int i=0; i<n_threads; ++i){
    threads.emplace_back(pop_thread, i, std::ref(ts[i]), std::ref(n_got[i]));
  }
  
  for(int i=0; i<n_threads; ++i){
    threads[i].join();
  }
  double sum = 0;
  for(int i=0; i<n_threads; ++i){
    sum += ts[i];
    n_got_total += n_got[i];
  }
  if(n_got_total > 0) {
    std::cout << "foo" << std::endl;
  }
  return sum/n_threads;
}

int main(int argc, char** argv)
{
  std::vector<int> n_threadss{1, 10, 20, 30, 50, 100};

  std::cout << "Note *nano*seconds in output, not microseconds" << std::endl;
  for(int n_threads : n_threadss) {
    double t = get_mean_pop_time(n_threads);
    std::cout << "n_threads = " << n_threads << ", mean time (ns) = " << t << ", ratio = " << (t/n_threads) << std::endl;
  }
  
}
