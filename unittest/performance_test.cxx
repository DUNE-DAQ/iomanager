/**
 * @file performance_test.cxx Performance Unit Tests
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2021.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/IOManager.hpp"

#include "serialization/Serialization.hpp"

#define BOOST_TEST_MODULE performance_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <atomic>
#include <functional>
#include <future>
#include <string>
#include <utility>
#include <vector>

using namespace dunedaq::iomanager;

namespace dunedaq {
struct data_t
{
  std::vector<uint8_t> d; // NOLINT(build/unsigned)
  data_t() = default;
  data_t(unsigned int size, uint8_t c) // NOLINT(build/unsigned)
    : d(size, c)
  {
    ;
  }
  DUNE_DAQ_SERIALIZE(data_t, d);
};
DUNE_DAQ_SERIALIZABLE(data_t);
} // namespace dunedaq

BOOST_AUTO_TEST_SUITE(performance_test)

struct ConfigurationTestFixture
{
  ConfigurationTestFixture()
  {
    setenv("DUNEDAQ_PARTITION", "performance_t", 0);

    conn_ep_s = Endpoint{ "data_t", "performance_test", "network_s", {}, Direction::kOutput };
    conn_ep_r = Endpoint{ "data_t", "performance_test", "network_r", {}, Direction::kInput };
    queue_ep = Endpoint{ "data_t", "performance_test", "queue", {}, Direction::kUnspecified };

    dunedaq::iomanager::Queues_t queues;
    queues.emplace_back(QueueConfig{ "test_queue", { queue_ep }, QueueType::kFollySPSCQueue, 50 });

    dunedaq::iomanager::Connections_t connections;
    connections.emplace_back(Connection{ conn_ep_r, { conn_ep_s }, "inproc://foo", ConnectionType::kSendRecv });
    IOManager::get()->configure(queues, connections);
  }
  ~ConfigurationTestFixture() { IOManager::get()->reset(); }

  ConfigurationTestFixture(ConfigurationTestFixture const&) = delete;
  ConfigurationTestFixture(ConfigurationTestFixture&&) = delete;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture const&) = delete;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture&&) = delete;

  dunedaq::iomanager::Endpoint conn_ep_s;
  dunedaq::iomanager::Endpoint conn_ep_r;
  dunedaq::iomanager::Endpoint queue_ep;
  const size_t n_sends = 10000;
  const size_t message_size = 55680;
};

BOOST_FIXTURE_TEST_CASE(CallbackRegistrationNetwork, ConfigurationTestFixture)
{
  std::atomic<unsigned int> received_count = 0;
  std::function<void(dunedaq::data_t)> callback = [&](dunedaq::data_t) { ++received_count; }; // NOLINT

  IOManager::get()->add_callback<dunedaq::data_t>(conn_ep_r, callback);
  auto net_sender = IOManager::get()->get_sender<dunedaq::data_t>(conn_ep_s);
  auto start_time = std::chrono::steady_clock::now();
  for (unsigned int i = 0; i < n_sends; ++i) {
    dunedaq::data_t temp(message_size, i % 200);
    net_sender->send(std::move(temp), Sender::s_no_block);
  }
  BOOST_TEST_MESSAGE("Messages sent, waiting for receives");
  while (received_count < n_sends) {
    usleep(1000);
  }

  IOManager::get()->remove_callback<dunedaq::data_t>(conn_ep_r);
  auto stop_time = std::chrono::steady_clock::now();

  auto time = std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count();
  double rate = received_count.load() / static_cast<double>(time) * 1e6; // Hz

  BOOST_CHECK(rate > 0.);
  BOOST_TEST_MESSAGE("network callback rate " << rate << " Hz");
}

BOOST_FIXTURE_TEST_CASE(CallbackRegistrationQueue, ConfigurationTestFixture)
{
  std::atomic<unsigned int> received_count = 0;
  std::function<void(dunedaq::data_t)> callback = [&](dunedaq::data_t) { ++received_count; }; // NOLINT

  IOManager::get()->add_callback<dunedaq::data_t>(queue_ep, callback);
  auto queue_sender = IOManager::get()->get_sender<dunedaq::data_t>(queue_ep);
  auto start_time = std::chrono::steady_clock::now();
  for (unsigned int i = 0; i < n_sends; ++i) {
    dunedaq::data_t temp(message_size, i % 200);
    queue_sender->send(std::move(temp), std::chrono::milliseconds(1000));
  }
  BOOST_TEST_MESSAGE("Messages sent, waiting for receives");
  while (received_count < n_sends) {
    usleep(1000);
  }
  IOManager::get()->remove_callback<dunedaq::data_t>(queue_ep);
  auto stop_time = std::chrono::steady_clock::now();

  auto time = std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count();
  double rate = received_count.load() / static_cast<double>(time) * 1e6; // Hz

  BOOST_CHECK(rate > 0.);
  BOOST_TEST_MESSAGE("queue callback rate " << rate << " Hz");
}

BOOST_FIXTURE_TEST_CASE(DirectReadNetwork, ConfigurationTestFixture)
{
  std::atomic<unsigned int> received_count = 0;
  unsigned int total_send = n_sends;
  std::function<void()> recv_func = [&]() {
    do {
      auto mess = IOManager::get()->get_receiver<dunedaq::data_t>(conn_ep_r)->receive(std::chrono::milliseconds(10));
      ++received_count;
    } while (received_count.load() < total_send);
  };

  auto net_sender = IOManager::get()->get_sender<dunedaq::data_t>(conn_ep_s);
  auto rcv_ftr = std::async(std::launch::async, recv_func);

  auto start_time = std::chrono::steady_clock::now();
  for (unsigned int i = 0; i < total_send; ++i) {
    dunedaq::data_t temp(message_size, i % 200);
    net_sender->send(std::move(temp), dunedaq::iomanager::Sender::s_no_block);
  }
  BOOST_TEST_MESSAGE("Messages sent, waiting for receives");
  rcv_ftr.get();
  auto stop_time = std::chrono::steady_clock::now();

  auto time = std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count();
  double rate = received_count.load() / static_cast<double>(time) * 1e6; // Hz

  BOOST_CHECK(rate > 0.);
  BOOST_TEST_MESSAGE("network read rate " << rate << " Hz");
}

BOOST_FIXTURE_TEST_CASE(DirectReadQueue, ConfigurationTestFixture)
{
  std::atomic<unsigned int> received_count = 0;
  unsigned int total_send = n_sends;
  std::function<void()> recv_func = [&]() {
    do {
      auto mess = IOManager::get()->get_receiver<dunedaq::data_t>(queue_ep)->receive(std::chrono::milliseconds(10));
      ++received_count;
    } while (received_count.load() < total_send);
  };

  auto queue_sender = IOManager::get()->get_sender<dunedaq::data_t>(queue_ep);
  auto rcv_ftr = std::async(std::launch::async, recv_func);

  auto start_time = std::chrono::steady_clock::now();
  for (unsigned int i = 0; i < total_send; ++i) {
    dunedaq::data_t temp(message_size, i % 200);
    queue_sender->send(std::move(temp), std::chrono::milliseconds(10));
  }
  BOOST_TEST_MESSAGE("Messages sent, waiting for receives");
  rcv_ftr.get();
  auto stop_time = std::chrono::steady_clock::now();

  auto time = std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count();
  double rate = received_count.load() / static_cast<double>(time) * 1e6; // Hz

  BOOST_CHECK(rate > 0.);
  BOOST_TEST_MESSAGE("queue read rate " << rate << " Hz");
}

BOOST_AUTO_TEST_SUITE_END()
