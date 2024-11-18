#include "iomanager/CommonIssues.hpp"
#include "iomanager/queue/FollyQueue.hpp"
#include <chrono>
#include <iostream>

#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"
#include "opmonlib/TestOpMonManager.hpp"

using namespace std::chrono;

namespace dunedaq {
namespace iomanager {
struct IntWrapper
{
  int theInt;

  IntWrapper() = default;
  IntWrapper(int i)
    : theInt(i)
  {
  }
  virtual ~IntWrapper() = default;
  IntWrapper(IntWrapper const&) = default;
  IntWrapper& operator=(IntWrapper const&) = default;
  IntWrapper(IntWrapper&&) = default;
  IntWrapper& operator=(IntWrapper&&) = default;

  DUNE_DAQ_SERIALIZE(IntWrapper, theInt);
};

} // namespace iomanager

// Must be in dunedaq namespace only
DUNE_DAQ_SERIALIZABLE(iomanager::IntWrapper, "int_t");
} // namespace dunedaq

void pop_thread(int n, int64_t& t)
{
	auto receiver = dunedaq::get_iom_receiver<dunedaq::iomanager::IntWrapper>("bar" + std::to_string(n));

	int N = 5000;
	milliseconds timeout(0);
	auto start = steady_clock::now();

	for (int i = 0; i < N; ++i) {
		try {
			auto n = receiver->receive(timeout);
			std::cout << n.theInt << std::endl; // This line is never reached, but hopefully prevents the optimizer no-opping anything
		}
		catch (dunedaq::iomanager::TimeoutExpired&) {
			// This is expected
		}
	}
	auto end = steady_clock::now();
	t = duration_cast<microseconds>(end - start).count() / N;
}

double get_mean_pop_time(int n_threads)
{
	std::vector<std::thread> threads;
	std::vector<int64_t> ts(n_threads);
	for (int i = 0; i < n_threads; ++i) {
		threads.emplace_back(pop_thread, i, std::ref(ts[i]));
	}
	for (int i = 0; i < n_threads; ++i) {
		threads[i].join();
	}
	double sum = 0;
	for (int i = 0; i < n_threads; ++i) {
		sum += ts[i];
	}
	return sum / n_threads;
}

int main(int , char** )
{

	int max_n_threads = 100;

	dunedaq::iomanager::Connections_t connections;
	dunedaq::iomanager::Queues_t queues;
	for (int i = 0; i < max_n_threads; ++i) {
		queues.emplace_back(dunedaq::iomanager::QueueConfig{ {"bar" + std::to_string(i),"int_t"}, dunedaq::iomanager::QueueType::kFollyMPMCQueue, 1000 });
	}

	dunedaq::opmonlib::TestOpMonManager opmgr;
	dunedaq::get_iomanager()->configure(queues, connections, false, 1000ms, opmgr);

	// Create all of the queues up front so the iomanager output is all in one place and not interspersed with the results
	for (int i = 0; i < max_n_threads; ++i) {
		auto receiver = dunedaq::get_iom_receiver<dunedaq::iomanager::IntWrapper>("bar" + std::to_string(i));
	}

	std::vector<int> n_threadss{ 1, 10, 20, 30, 50, 100 };

	for (int n_threads : n_threadss) {
		double t = get_mean_pop_time(n_threads);
		std::cout << "n_threads = " << n_threads << ", mean time (us) = " << t << ", ratio = " << (t / n_threads) << std::endl;
	}

}
