#include "iomanager/CommonIssues.hpp"
#include "iomanager/queue/FollyQueue.hpp"
#include <chrono>
#include <iostream>

#include "folly/concurrency/DynamicBoundedQueue.h"

using namespace std::chrono;

class MyTimeout {};

  
class ThrowingQueue
{
public:
  ThrowingQueue() {}

  void pop(int& dr)
  {
    if(!q.try_dequeue_for(dr, milliseconds(0))) {
      throw MyTimeout();
    }
  }
  
private:
  folly::DMPMCQueue<int, true> q{1000};  
};

void pop_thread(int , int64_t& t, int& n_got)
{
  int N = 5000;

  ThrowingQueue q;
  
  auto start = steady_clock::now();

  for(int i=0; i<N; ++i){
    int dr;
    try {
      q.pop(dr);
      ++n_got;
    } catch (MyTimeout&) {

    }
  }
  auto end = steady_clock::now();
  t=duration_cast<microseconds>(end - start).count()/N;
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

int main(int , char** )
{
  std::vector<int> n_threadss{1, 10, 20, 30, 50, 100};

  for(int n_threads : n_threadss) {
    double t = get_mean_pop_time(n_threads);
    std::cout << "n_threads = " << n_threads << ", mean time (us) = " << t << ", ratio = " << (t/n_threads) << std::endl;
  }
  
}
