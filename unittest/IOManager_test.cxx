/**
 * @file IOManager_test.cxx IOManager Unit Tests
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2021.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/IOManager.hpp"

#include "serialization/Serialization.hpp"

#define BOOST_TEST_MODULE IOManager_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <string>
#include <vector>

using namespace dunedaq::iomanager;

namespace dunedaq {
namespace iomanager {
struct Data
{
  int d1;
  double d2;
  std::string d3;

  Data() = default;
  Data(int i, double d, std::string s)
    : d1(i)
    , d2(d)
    , d3(s)
  {}
  virtual ~Data() = default;
  Data(Data const&) = default;
  Data& operator=(Data const&) = default;
  Data(Data&&) = default;
  Data& operator=(Data&&) = default;

  DUNE_DAQ_SERIALIZE(Data, d1, d2, d3);
};

struct NonCopyableData
{
  int d1;
  double d2;
  std::string d3;

  NonCopyableData() = default;
  NonCopyableData(int i, double d, std::string s)
    : d1(i)
    , d2(d)
    , d3(s)
  {}
  virtual ~NonCopyableData() = default;
  NonCopyableData(NonCopyableData const&) = delete;
  NonCopyableData& operator=(NonCopyableData const&) = delete;
  NonCopyableData(NonCopyableData&&) = default;
  NonCopyableData& operator=(NonCopyableData&&) = default;

  DUNE_DAQ_SERIALIZE(NonCopyableData, d1, d2, d3);
};

struct NonSerializableData
{
  int d1;
  double d2;
  std::string d3;

  NonSerializableData() = default;
  NonSerializableData(int i, double d, std::string s)
    : d1(i)
    , d2(d)
    , d3(s)
  {}
  virtual ~NonSerializableData() = default;
  NonSerializableData(NonSerializableData const&) = default;
  NonSerializableData& operator=(NonSerializableData const&) = default;
  NonSerializableData(NonSerializableData&&) = default;
  NonSerializableData& operator=(NonSerializableData&&) = default;
};

struct NonSerializableNonCopyable
{
  int d1;
  double d2;
  std::string d3;

  NonSerializableNonCopyable() = default;
  NonSerializableNonCopyable(int i, double d, std::string s)
    : d1(i)
    , d2(d)
    , d3(s)
  {}
  virtual ~NonSerializableNonCopyable() = default;
  NonSerializableNonCopyable(NonSerializableNonCopyable const&) = delete;
  NonSerializableNonCopyable& operator=(NonSerializableNonCopyable const&) = delete;
  NonSerializableNonCopyable(NonSerializableNonCopyable&&) = default;
  NonSerializableNonCopyable& operator=(NonSerializableNonCopyable&&) = default;
};

} // namespace iomanager

// Must be in dunedaq namespace only
DUNE_DAQ_SERIALIZABLE(iomanager::Data);
DUNE_DAQ_SERIALIZABLE(iomanager::NonCopyableData);

} // namespace dunedaq

BOOST_AUTO_TEST_SUITE(IOManager_test)

struct ConfigurationTestFixture
{
  ConfigurationTestFixture()
  {
    ConnectionIds_t connections;
    connections.emplace_back(
      ConnectionId{ "test_queue", ServiceType::kQueue, "NonCopyableData", "queue://StdDeQueue:10" });
    connections.emplace_back(
      ConnectionId{ "test_connection", ServiceType::kNetwork, "NonCopyableData", "inproc://foo" });
    iom.configure(connections);
    conn_ref = ConnectionRef{ "network", "test_connection" };
    queue_ref = ConnectionRef{ "queue", "test_queue" };
  }
  ~ConfigurationTestFixture() { IOManager::reset(); }

  ConfigurationTestFixture(ConfigurationTestFixture const&) = default;
  ConfigurationTestFixture(ConfigurationTestFixture&&) = default;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture const&) = default;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture&&) = default;

  IOManager iom;
  ConnectionRef conn_ref;
  ConnectionRef queue_ref;
};

BOOST_AUTO_TEST_CASE(CopyAndMoveSemantics)
{
  BOOST_REQUIRE(!std::is_copy_constructible_v<IOManager>);
  BOOST_REQUIRE(!std::is_copy_assignable_v<IOManager>);
  BOOST_REQUIRE(!std::is_move_constructible_v<IOManager>);
  BOOST_REQUIRE(!std::is_move_assignable_v<IOManager>);
}

BOOST_AUTO_TEST_CASE(Directionality) 
{
  IOManager iom;
 ConnectionIds_t connections;
  connections.emplace_back(ConnectionId{ "test_connection", ServiceType::kNetwork, "Data", "inproc://foo" });
  iom.configure(connections);
  ConnectionRef unspecified_ref = ConnectionRef{ "unspecified", "test_connection", {} };
  ConnectionRef input_ref = ConnectionRef{ "input", "test_connection", {}, Direction::kInput };
  ConnectionRef output_ref = ConnectionRef{ "output", "test_connection", {}, Direction::kOutput };

  // Unspecified is always ok
  auto sender = iom.get_sender<Data>(unspecified_ref);
  auto receiver = iom.get_receiver<Data>(unspecified_ref);

  // Input can only be receiver
  BOOST_REQUIRE_EXCEPTION(iom.get_sender<Data>(input_ref),
                          ConnectionDirectionMismatch,
                          [](ConnectionDirectionMismatch const&) { return true; });
  receiver = iom.get_receiver<Data>(input_ref);

  // Output can only be sender
  sender = iom.get_sender<Data>(output_ref);
  BOOST_REQUIRE_EXCEPTION(iom.get_receiver<Data>(output_ref),
                          ConnectionDirectionMismatch,
                          [](ConnectionDirectionMismatch const&) { return true; });

  iom.reset();
}

BOOST_FIXTURE_TEST_CASE(SimpleSendReceive, ConfigurationTestFixture)
{
  auto net_sender = iom.get_sender<Data>(conn_ref);
  auto net_receiver = iom.get_receiver<Data>(conn_ref);
  auto q_sender = iom.get_sender<Data>(queue_ref);
  auto q_receiver = iom.get_receiver<Data>(queue_ref);

  Data sent_nw(56, 26.5, "test1");
  Data sent_q(57, 27.5, "test2");
  net_sender->send(sent_nw, dunedaq::iomanager::Sender::s_no_block);

  auto ret = net_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 56);
  BOOST_CHECK_EQUAL(ret.d2, 26.5);
  BOOST_CHECK_EQUAL(ret.d3, "test1");

  q_sender->send(sent_q, std::chrono::milliseconds(10));

  ret = q_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 57);
  BOOST_CHECK_EQUAL(ret.d2, 27.5);
  BOOST_CHECK_EQUAL(ret.d3, "test2");
}

BOOST_FIXTURE_TEST_CASE(NonSerializableSendReceive, ConfigurationTestFixture)
{
  auto net_sender = iom.get_sender<NonSerializableData>(conn_ref);
  auto net_receiver = iom.get_receiver<NonSerializableData>(conn_ref);
  auto q_sender = iom.get_sender<NonSerializableData>(queue_ref);
  auto q_receiver = iom.get_receiver<NonSerializableData>(queue_ref);

  NonSerializableData sent_nw(56, 26.5, "test1");
  NonSerializableData sent_q(57, 27.5, "test2");
  net_sender->send(sent_nw, dunedaq::iomanager::Sender::s_no_block);

  auto ret = net_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 0);
  BOOST_CHECK_EQUAL(ret.d2, 0);
  BOOST_CHECK_EQUAL(ret.d3, "");

  q_sender->send(sent_q, std::chrono::milliseconds(10));

  ret = q_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 57);
  BOOST_CHECK_EQUAL(ret.d2, 27.5);
  BOOST_CHECK_EQUAL(ret.d3, "test2");
}

BOOST_FIXTURE_TEST_CASE(NonCopyableSendReceive, ConfigurationTestFixture)
{
  auto net_sender = iom.get_sender<NonCopyableData>(conn_ref);
  auto net_receiver = iom.get_receiver<NonCopyableData>(conn_ref);
  auto q_sender = iom.get_sender<NonCopyableData>(queue_ref);
  auto q_receiver = iom.get_receiver<NonCopyableData>(queue_ref);

  NonCopyableData sent_nw(56, 26.5, "test1");
  NonCopyableData sent_q(57, 27.5, "test2");
  net_sender->send(sent_nw, dunedaq::iomanager::Sender::s_no_block);

  auto ret = net_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 56);
  BOOST_CHECK_EQUAL(ret.d2, 26.5);
  BOOST_CHECK_EQUAL(ret.d3, "test1");

  q_sender->send(sent_q, std::chrono::milliseconds(10));

  ret = q_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 57);
  BOOST_CHECK_EQUAL(ret.d2, 27.5);
  BOOST_CHECK_EQUAL(ret.d3, "test2");
}

BOOST_FIXTURE_TEST_CASE(NonSerializableNonCopyableSendReceive, ConfigurationTestFixture)
{
  auto net_sender = iom.get_sender<NonSerializableNonCopyable>(conn_ref);
  auto net_receiver = iom.get_receiver<NonSerializableNonCopyable>(conn_ref);
  auto q_sender = iom.get_sender<NonSerializableNonCopyable>(queue_ref);
  auto q_receiver = iom.get_receiver<NonSerializableNonCopyable>(queue_ref);

  NonSerializableNonCopyable sent_nw(56, 26.5, "test1");
  NonSerializableNonCopyable sent_q(57, 27.5, "test2");
  net_sender->send(sent_nw, dunedaq::iomanager::Sender::s_no_block);

  auto ret = net_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 0);
  BOOST_CHECK_EQUAL(ret.d2, 0);
  BOOST_CHECK_EQUAL(ret.d3, "");

  q_sender->send(sent_q, std::chrono::milliseconds(10));

  ret = q_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 57);
  BOOST_CHECK_EQUAL(ret.d2, 27.5);
  BOOST_CHECK_EQUAL(ret.d3, "test2");
}

BOOST_FIXTURE_TEST_CASE(CallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = iom.get_sender<Data>(conn_ref);
  auto q_sender = iom.get_sender<Data>(queue_ref);

  Data sent_data_nw(56, 26.5, "test1");
  Data sent_data_q(57, 27.5, "test2");
  Data recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(Data&)> callback = [&](Data& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  iom.add_callback<Data>(conn_ref, callback);
  iom.add_callback<Data>(queue_ref, callback);

  usleep(1000);

  net_sender->send(sent_data_nw, dunedaq::iomanager::Sender::s_no_block);

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 56);
  BOOST_CHECK_EQUAL(recv_data.d2, 26.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test1");

  has_received_data = false;
  q_sender->send(sent_data_q, std::chrono::milliseconds(10));

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 57);
  BOOST_CHECK_EQUAL(recv_data.d2, 27.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test2");

  iom.remove_callback<Data>(conn_ref);
  iom.remove_callback<Data>(queue_ref);
}

BOOST_FIXTURE_TEST_CASE(NonCopyableCallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = iom.get_sender<NonCopyableData>(conn_ref);
  auto q_sender = iom.get_sender<NonCopyableData>(queue_ref);

  NonCopyableData sent_data_nw(56, 26.5, "test1");
  NonCopyableData sent_data_q(57, 27.5, "test2");
  NonCopyableData recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(NonCopyableData&)> callback = [&](NonCopyableData& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  iom.add_callback<NonCopyableData>(conn_ref, callback);
  iom.add_callback<NonCopyableData>(queue_ref, callback);

  usleep(1000);

  net_sender->send(sent_data_nw, dunedaq::iomanager::Sender::s_no_block);

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 56);
  BOOST_CHECK_EQUAL(recv_data.d2, 26.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test1");

  has_received_data = false;
  q_sender->send(sent_data_q, std::chrono::milliseconds(10));

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 57);
  BOOST_CHECK_EQUAL(recv_data.d2, 27.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test2");

  iom.remove_callback<NonCopyableData>(conn_ref);
  iom.remove_callback<NonCopyableData>(queue_ref);
}

BOOST_FIXTURE_TEST_CASE(NonSerializableCallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = iom.get_sender<NonSerializableData>(conn_ref);
  auto q_sender = iom.get_sender<NonSerializableData>(queue_ref);

  NonSerializableData sent_data_nw(56, 26.5, "test1");
  NonSerializableData sent_data_q(57, 27.5, "test2");
  NonSerializableData recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(NonSerializableData&)> callback = [&](NonSerializableData& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  iom.add_callback<NonSerializableData>(conn_ref, callback);
  iom.add_callback<NonSerializableData>(queue_ref, callback);

  usleep(1000);

  net_sender->send(sent_data_nw, dunedaq::iomanager::Sender::s_no_block);

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 0);
  BOOST_CHECK_EQUAL(recv_data.d2, 0.0);
  BOOST_CHECK_EQUAL(recv_data.d3, "");

  // Have to stop the callback from endlessly setting recv_data to default-constructed object
  iom.remove_callback<NonSerializableData>(conn_ref);
  has_received_data = false;
  q_sender->send(sent_data_q, std::chrono::milliseconds(10));

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 57);
  BOOST_CHECK_EQUAL(recv_data.d2, 27.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test2");

  iom.remove_callback<NonSerializableData>(queue_ref);
}

BOOST_FIXTURE_TEST_CASE(NonSerializableNonCopyableCallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = iom.get_sender<NonSerializableNonCopyable>(conn_ref);
  auto q_sender = iom.get_sender<NonSerializableNonCopyable>(queue_ref);

  NonSerializableNonCopyable sent_data_nw(56, 26.5, "test1");
  NonSerializableNonCopyable sent_data_q(57, 27.5, "test2");
  NonSerializableNonCopyable recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(NonSerializableNonCopyable&)> callback = [&](NonSerializableNonCopyable& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  iom.add_callback<NonSerializableNonCopyable>(conn_ref, callback);
  iom.add_callback<NonSerializableNonCopyable>(queue_ref, callback);

  usleep(1000);

  net_sender->send(sent_data_nw, dunedaq::iomanager::Sender::s_no_block);

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 0);
  BOOST_CHECK_EQUAL(recv_data.d2, 0.0);
  BOOST_CHECK_EQUAL(recv_data.d3, "");

  // Have to stop the callback from endlessly setting recv_data to default-constructed object
  iom.remove_callback<NonSerializableNonCopyable>(conn_ref);
  has_received_data = false;
  q_sender->send(sent_data_q, std::chrono::milliseconds(10));

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 57);
  BOOST_CHECK_EQUAL(recv_data.d2, 27.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test2");

  iom.remove_callback<NonSerializableNonCopyable>(queue_ref);
}

BOOST_AUTO_TEST_SUITE_END()
