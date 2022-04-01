/**
 * @file performance_test.cxx Performance Unit Tests
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2021.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/IOManager.hpp"

#include "appfwk/QueueRegistry.hpp"
#include "appfwk/app/Nljs.hpp"
#include "networkmanager/NetworkManager.hpp"
#include "networkmanager/nwmgr/Structs.hpp"
#include "serialization/Serialization.hpp"

#define BOOST_TEST_MODULE performance_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <atomic>
#include <functional>
#include <future>
#include <string>
#include <vector>

using namespace dunedaq::iomanager;

namespace dunedaq {
struct data_t
{
  std::vector<uint8_t> d;
  data_t() = default;
  data_t(unsigned int size, uint8_t c)
    : d(size, c)
  {
    ;
  }
  DUNE_DAQ_SERIALIZE(data_t, d);
};
DUNE_DAQ_SERIALIZABLE(data_t);
}

BOOST_AUTO_TEST_SUITE(performance_test)

struct ConfigurationTestFixture
{
  ConfigurationTestFixture()
  {
    dunedaq::networkmanager::nwmgr::Connections nwCfg;
    dunedaq::networkmanager::nwmgr::Connection testConn;
    testConn.name = "test_connection";
    testConn.address = "inproc://foo";
    nwCfg.push_back(testConn);
    dunedaq::networkmanager::NetworkManager::get().configure(nwCfg);

    std::map<std::string, dunedaq::appfwk::QueueConfig> config_map;
    dunedaq::appfwk::QueueConfig qspec;
    qspec.kind = dunedaq::appfwk::QueueConfig::queue_kind::kStdDeQueue;
    qspec.capacity = 10;
    config_map["test_queue"] = qspec;
    dunedaq::appfwk::QueueRegistry::get().configure(config_map);

    dunedaq::iomanager::ConnectionIds_t connections;
    connections.emplace_back(ConnectionId{ "test_queue", ServiceType::kQueue, "data_t", "queue://StdDeque:10" });
    connections.emplace_back(
      ConnectionId{ "test_connection", ServiceType::kNetwork, "data_t", "inproc://foo" });
    iom.configure(connections);
    conn_ref = ConnectionRef{ "network", "test_connection" };
    queue_ref = ConnectionRef{ "queue", "test_queue" };
  }
  ~ConfigurationTestFixture()
  {
    dunedaq::networkmanager::NetworkManager::get().reset();
    dunedaq::appfwk::QueueRegistry::get().reset();
  }

  ConfigurationTestFixture(ConfigurationTestFixture const&) = default;
  ConfigurationTestFixture(ConfigurationTestFixture&&) = default;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture const&) = default;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture&&) = default;

  dunedaq::iomanager::IOManager iom;
  dunedaq::iomanager::ConnectionRef conn_ref;
  dunedaq::iomanager::ConnectionRef queue_ref;
};

BOOST_FIXTURE_TEST_CASE(CallbackRegistrationNetwork, ConfigurationTestFixture)
{
  std::atomic<unsigned int> received_count = 0;
  std::function<void(dunedaq::data_t)> callback = [&](dunedaq::data_t) { ++received_count; };

  iom.add_callback<dunedaq::data_t>(conn_ref, callback);
  auto net_sender = iom.get_sender<dunedaq::data_t>(conn_ref);
  auto start_time = std::chrono::steady_clock::now();
  for (unsigned int i = 0; i < 100000; ++i) {
    dunedaq::data_t temp(55680, i % 200);
    net_sender->send(temp, Sender::s_no_block);
  }

  iom.remove_callback<dunedaq::data_t>(conn_ref);
  auto stop_time = std::chrono::steady_clock::now();

  auto time = std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count();
  double rate = received_count.load() / (double)time * 1e6; // Hz

  BOOST_CHECK(rate > 0.);
  BOOST_TEST_MESSAGE("callback rate " << rate << " Hz");
}

BOOST_FIXTURE_TEST_CASE(CallbackRegistrationQueue, ConfigurationTestFixture)
{
  std::atomic<unsigned int> received_count = 0;
  std::function<void(dunedaq::data_t)> callback = [&](dunedaq::data_t) { ++received_count; };

  iom.add_callback<dunedaq::data_t>(queue_ref, callback);
  auto queue_sender = iom.get_sender<dunedaq::data_t>(queue_ref);
  auto start_time = std::chrono::steady_clock::now();
  for (unsigned int i = 0; i < 100000; ++i) {
    dunedaq::data_t temp(55680, i % 200);
    queue_sender->send(temp, dunedaq::iomanager::Sender::s_no_block);
  }
  iom.remove_callback<dunedaq::data_t>(queue_ref);
  auto stop_time = std::chrono::steady_clock::now();

  auto time = std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count();
  double rate = received_count.load() / (double)time * 1e6; // Hz

  BOOST_CHECK(rate > 0.);
  BOOST_TEST_MESSAGE("queue rate " << rate << " Hz");
}

BOOST_FIXTURE_TEST_CASE(DirectReadNetwork, ConfigurationTestFixture)
{
  std::atomic<unsigned int> received_count = 0;
  unsigned int total_send = 100000;
  std::function<void()> recv_func = [&]() {
    do {
      auto mess = iom.get_receiver<dunedaq::data_t>(conn_ref)->receive(std::chrono::milliseconds(10));
      ++received_count;
    } while (received_count.load() < total_send);
  };

  auto net_sender = iom.get_sender<dunedaq::data_t>(conn_ref);
  auto rcv_ftr = std::async(std::launch::async, recv_func);

  auto start_time = std::chrono::steady_clock::now();
  for (unsigned int i = 0; i < total_send; ++i) {
    dunedaq::data_t temp(55680, i % 200);
    net_sender->send(temp, dunedaq::iomanager::Sender::s_no_block);
  }
  rcv_ftr.get();
  auto stop_time = std::chrono::steady_clock::now();

  auto time = std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count();
  double rate = received_count.load() / (double)time * 1e6; // Hz

  BOOST_CHECK(rate > 0.);
  BOOST_TEST_MESSAGE("direct read rate " << rate << " Hz");
}

BOOST_FIXTURE_TEST_CASE(DirectReadQueue, ConfigurationTestFixture)
{
  std::atomic<unsigned int> received_count = 0;
  unsigned int total_send = 100000;
  std::function<void()> recv_func = [&]() {
    do {
      auto mess = iom.get_receiver<dunedaq::data_t>(queue_ref)->receive(std::chrono::milliseconds(10));
      ++received_count;
    } while (received_count.load() < total_send);
  };

  auto queue_sender = iom.get_sender<dunedaq::data_t>(queue_ref);
  auto rcv_ftr = std::async(std::launch::async, recv_func);
  
  auto start_time = std::chrono::steady_clock::now();
  for (unsigned int i = 0; i < total_send; ++i) {
    dunedaq::data_t temp(55680, i % 200);
    queue_sender->send(temp, std::chrono::milliseconds(10));
  }
  rcv_ftr.get();
  auto stop_time = std::chrono::steady_clock::now();

  auto time = std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count();
  double rate = received_count.load() / (double)time * 1e6; // Hz

  BOOST_CHECK(rate > 0.);
  BOOST_TEST_MESSAGE("queue rate " << rate << " Hz");
}

BOOST_AUTO_TEST_SUITE_END()
