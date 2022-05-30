#include "iomanager/CommonIssues.hpp"
#include "iomanager/FollyQueue.hpp"
#include <chrono>
#include <iostream>

#include "iomanager/ConnectionId.hpp"
#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"

using namespace std::chrono;

void pop_thread(int n, int64_t& t)
{
  dunedaq::iomanager::ConnectionRef cref;
  cref.uid = "bar" + std::to_string(n);

  auto receiver = dunedaq::get_iom_receiver<int>(cref);

  int N = 5000;
  milliseconds timeout(0);
  auto start = steady_clock::now();
  
  for(int i=0; i<N; ++i){
    try {
      int n = receiver->receive(timeout);
      std::cout << n << std::endl; // This line is never reached, but hopefully prevents the optimizer no-opping anything
    } catch (dunedaq::iomanager::TimeoutExpired&) {
      // This is expected
    }
  }
  auto end = steady_clock::now();
  t=duration_cast<microseconds>(end - start).count()/N;
}

double get_mean_pop_time(int n_threads)
{
  std::vector<std::thread> threads;
  std::vector<int64_t> ts(n_threads);
  for(int i=0; i<n_threads; ++i){
    threads.emplace_back(pop_thread, i, std::ref(ts[i]));
  }
  for(int i=0; i<n_threads; ++i){
    threads[i].join();
  }
  double sum = 0;
  for(int i=0; i<n_threads; ++i){
    sum += ts[i];
  }
  return sum/n_threads;
}

int main(int argc, char** argv)
{

  int max_n_threads = 100;
  
  dunedaq::iomanager::ConnectionIds_t connections;
  for(int i=0; i<max_n_threads; ++i){
    dunedaq::iomanager::ConnectionId cid;
    cid.service_type = dunedaq::iomanager::ServiceType::kQueue;
    cid.uid = "bar" + std::to_string(i);
    cid.uri = "queue://FollyMPMC:1000";
    cid.data_type = "int";
    connections.push_back(cid);
  }
  dunedaq::get_iomanager()->configure(connections);

  // Create all of the queues up front so the iomanager output is all in one place and not interspersed with the results
  for(int i=0; i<max_n_threads; ++i){
    dunedaq::iomanager::ConnectionRef cref;
    cref.uid = "bar" + std::to_string(i);
    auto receiver = dunedaq::get_iom_receiver<int>(cref);
  }
  
  std::vector<int> n_threadss{1, 10, 20, 30, 50, 100};

  for(int n_threads : n_threadss) {
    double t = get_mean_pop_time(n_threads);
    std::cout << "n_threads = " << n_threads << ", mean time (us) = " << t << ", ratio = " << (t/n_threads) << std::endl;
  }
  
}
