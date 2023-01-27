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
#include <utility>
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
  {
  }
  virtual ~Data() = default;
  Data(Data const&) = default;
  Data& operator=(Data const&) = default;
  Data(Data&&) = default;
  Data& operator=(Data&&) = default;

  DUNE_DAQ_SERIALIZE(Data, d1, d2, d3);
};
struct Data2
{
  int d1;
  double d2;
  std::string d3;

  Data2() = default;
  Data2(int i, double d, std::string s)
    : d1(i)
    , d2(d)
    , d3(s)
  {
  }
  virtual ~Data2() = default;
  Data2(Data2 const&) = default;
  Data2& operator=(Data2 const&) = default;
  Data2(Data2&&) = default;
  Data2& operator=(Data2&&) = default;

  DUNE_DAQ_SERIALIZE(Data2, d1, d2, d3);
};
struct Data3
{
  int d1;
  double d2;
  std::string d3;

  Data3() = default;
  Data3(int i, double d, std::string s)
    : d1(i)
    , d2(d)
    , d3(s)
  {
  }
  virtual ~Data3() = default;
  Data3(Data3 const&) = default;
  Data3& operator=(Data3 const&) = default;
  Data3(Data3&&) = default;
  Data3& operator=(Data3&&) = default;

  DUNE_DAQ_SERIALIZE(Data3, d1, d2, d3);
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
  {
  }
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
  {
  }
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
  {
  }
  virtual ~NonSerializableNonCopyable() = default;
  NonSerializableNonCopyable(NonSerializableNonCopyable const&) = delete;
  NonSerializableNonCopyable& operator=(NonSerializableNonCopyable const&) = delete;
  NonSerializableNonCopyable(NonSerializableNonCopyable&&) = default;
  NonSerializableNonCopyable& operator=(NonSerializableNonCopyable&&) = default;
};

} // namespace iomanager

// Must be in dunedaq namespace only
DUNE_DAQ_SERIALIZABLE(iomanager::Data, "data_t");
DUNE_DAQ_SERIALIZABLE(iomanager::Data2, "data2_t");
DUNE_DAQ_SERIALIZABLE(iomanager::Data3, "data3_t");
DUNE_DAQ_SERIALIZABLE(iomanager::NonCopyableData, "data_t");

// Note: Using the same data type string is bad, don't do it for real data types!
template<>
inline std::string
datatype_to_string<NonSerializableData>()
{
  return "data_t";
}
template<>
inline std::string
datatype_to_string<NonSerializableNonCopyable>()
{
  return "data_t";
}
} // namespace dunedaq

BOOST_AUTO_TEST_SUITE(IOManager_test)

struct ConfigurationTestFixture
{
  ConfigurationTestFixture()
  {
    setenv("DUNEDAQ_PARTITION", "IOManager_t", 0);

    conn_id = ConnectionId{ "network", "data_t" };
    queue_id = ConnectionId{ "queue", "data_t" };
    pub1_id = ConnectionId{ "pub1", "data2_t" };
    pub2_id = ConnectionId{ "pub2", "data2_t" };
    pub3_id = ConnectionId{ "pub3", "data3_t" };
    sub1_id = ConnectionId{ "pub.*", "data2_t" };
    sub2_id = ConnectionId{ "pub2", "data2_t" };
    sub3_id = ConnectionId{ "pub.*", "data3_t" };

    dunedaq::iomanager::Queues_t queues;
    queues.emplace_back(QueueConfig{ queue_id, QueueType::kFollySPSCQueue, 50 });

    dunedaq::iomanager::Connections_t connections;
    connections.emplace_back(Connection{ conn_id, "inproc://foo", ConnectionType::kSendRecv });
    connections.emplace_back(Connection{ pub1_id, "inproc://bar", ConnectionType::kPubSub });
    connections.emplace_back(Connection{ pub2_id, "inproc://baz", ConnectionType::kPubSub });
    connections.emplace_back(Connection{ pub3_id, "inproc://qui", ConnectionType::kPubSub });
    IOManager::get()->configure(queues, connections, false);
  }
  ~ConfigurationTestFixture() { IOManager::get()->reset(); }

  ConfigurationTestFixture(ConfigurationTestFixture const&) = default;
  ConfigurationTestFixture(ConfigurationTestFixture&&) = default;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture const&) = default;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture&&) = default;

  ConnectionId conn_id;
  ConnectionId queue_id;
  ConnectionId pub1_id;
  ConnectionId pub2_id;
  ConnectionId pub3_id;
  ConnectionId sub1_id;
  ConnectionId sub2_id;
  ConnectionId sub3_id;
};

BOOST_AUTO_TEST_CASE(CopyAndMoveSemantics)
{
  BOOST_REQUIRE(!std::is_copy_constructible_v<IOManager>);
  BOOST_REQUIRE(!std::is_copy_assignable_v<IOManager>);
  BOOST_REQUIRE(!std::is_move_constructible_v<IOManager>);
  BOOST_REQUIRE(!std::is_move_assignable_v<IOManager>);
}

BOOST_AUTO_TEST_CASE(Singleton)
{
  auto iom = IOManager::get();
  auto another_iom = IOManager::get();

  BOOST_REQUIRE_EQUAL(iom.get(), another_iom.get());
}

BOOST_FIXTURE_TEST_CASE(DatatypeMismatchException, ConfigurationTestFixture)
{
  ConnectionId bad_id{ "network", "baddata_t" };

  auto sender = IOManager::get()->get_sender<Data>(conn_id);
  BOOST_REQUIRE_EXCEPTION(
    IOManager::get()->get_sender<Data>(bad_id), DatatypeMismatch, [](DatatypeMismatch const&) { return true; });

  auto receiver = IOManager::get()->get_receiver<Data>(conn_id);
  BOOST_REQUIRE_EXCEPTION(
    IOManager::get()->get_receiver<Data>(bad_id), DatatypeMismatch, [](DatatypeMismatch const&) { return true; });
}

BOOST_FIXTURE_TEST_CASE(SimpleSendReceive, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<Data>(conn_id);
  auto net_receiver = IOManager::get()->get_receiver<Data>(conn_id);
  auto q_sender = IOManager::get()->get_sender<Data>(queue_id);
  auto q_receiver = IOManager::get()->get_receiver<Data>(queue_id);

  Data sent_nw(56, 26.5, "test1");
  Data sent_q(57, 27.5, "test2");
  net_sender->send(std::move(sent_nw), dunedaq::iomanager::Sender::s_no_block);

  auto ret = net_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 56);
  BOOST_CHECK_EQUAL(ret.d2, 26.5);
  BOOST_CHECK_EQUAL(ret.d3, "test1");

  q_sender->send(std::move(sent_q), std::chrono::milliseconds(10));

  ret = q_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 57);
  BOOST_CHECK_EQUAL(ret.d2, 27.5);
  BOOST_CHECK_EQUAL(ret.d3, "test2");
}

BOOST_FIXTURE_TEST_CASE(SimplePubSub, ConfigurationTestFixture)
{
  auto pub1_sender = IOManager::get()->get_sender<Data2>(pub1_id);
  auto pub2_sender = IOManager::get()->get_sender<Data2>(pub2_id);
  auto pub3_sender = IOManager::get()->get_sender<Data3>(pub3_id);
  auto sub1_receiver = IOManager::get()->get_receiver<Data2>(sub1_id);
  auto sub2_receiver = IOManager::get()->get_receiver<Data2>(sub2_id);
  auto sub3_receiver = IOManager::get()->get_receiver<Data3>(sub3_id);

  // Sub1 is subscribed to all data_t publishers, Sub2 only to pub2, Sub3 to all data2_t
  Data2 sent_t1(56, 26.5, "test1");
  pub1_sender->send(std::move(sent_t1), dunedaq::iomanager::Sender::s_no_block);

  auto ret1 = sub1_receiver->receive(std::chrono::milliseconds(10));
  BOOST_REQUIRE_EXCEPTION(
    sub2_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(
    sub3_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_CHECK_EQUAL(ret1.d1, 56);
  BOOST_CHECK_EQUAL(ret1.d2, 26.5);
  BOOST_CHECK_EQUAL(ret1.d3, "test1");

  Data2 sent_t2(57, 27.5, "test2");
  pub2_sender->send(std::move(sent_t2), dunedaq::iomanager::Sender::s_no_block);

  ret1 = sub1_receiver->receive(std::chrono::milliseconds(10));
  auto ret2 = sub2_receiver->receive(std::chrono::milliseconds(10));
  BOOST_REQUIRE_EXCEPTION(
    sub3_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_CHECK_EQUAL(ret1.d1, 57);
  BOOST_CHECK_EQUAL(ret1.d2, 27.5);
  BOOST_CHECK_EQUAL(ret1.d3, "test2");
  BOOST_CHECK_EQUAL(ret2.d1, 57);
  BOOST_CHECK_EQUAL(ret2.d2, 27.5);
  BOOST_CHECK_EQUAL(ret2.d3, "test2");

  Data3 sent_t3(58, 28.5, "test3");
  pub3_sender->send(std::move(sent_t3), dunedaq::iomanager::Sender::s_no_block);

  BOOST_REQUIRE_EXCEPTION(
    sub1_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(
    sub2_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  auto ret3 = sub3_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret3.d1, 58);
  BOOST_CHECK_EQUAL(ret3.d2, 28.5);
  BOOST_CHECK_EQUAL(ret3.d3, "test3");
}

BOOST_FIXTURE_TEST_CASE(PubSubWithTopic, ConfigurationTestFixture)
{
  auto pub1_sender = IOManager::get()->get_sender<Data2>(pub1_id);
  auto pub2_sender = IOManager::get()->get_sender<Data2>(pub2_id);
  auto sub1_receiver = IOManager::get()->get_receiver<Data2>(sub1_id);
  auto sub2_receiver = IOManager::get()->get_receiver<Data2>(sub2_id);

  sub1_receiver->subscribe("sub1_topic");
  sub2_receiver->subscribe("sub2_topic");

  // Sub1 is subscribed to all data_t publishers, Sub2 only to pub2
  Data2 sent_t0(54, 24.5, "test0");
  pub1_sender->send(std::move(sent_t0), dunedaq::iomanager::Sender::s_no_block);
  auto ret1 = sub1_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret1.d1, 54);
  BOOST_CHECK_EQUAL(ret1.d2, 24.5);
  BOOST_CHECK_EQUAL(ret1.d3, "test0");
  BOOST_REQUIRE_EXCEPTION(
    sub2_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });

  sub1_receiver->unsubscribe("data2_t");
  Data2 sent_t1(55, 25.5, "test1");
  pub1_sender->send(std::move(sent_t1), dunedaq::iomanager::Sender::s_no_block);
  BOOST_REQUIRE_EXCEPTION(
    sub1_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(
    sub2_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });

  Data2 sent_t2(56, 26.5, "test2");
  pub1_sender->send_with_topic(std::move(sent_t2), dunedaq::iomanager::Sender::s_no_block, "test_topic");
  BOOST_REQUIRE_EXCEPTION(
    sub1_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(
    sub2_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });

  Data2 sent_t3{ 57, 27.5, "test3" };
  pub1_sender->send_with_topic(std::move(sent_t3), dunedaq::iomanager::Sender::s_no_block, "sub1_topic");
  ret1 = sub1_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret1.d1, 57);
  BOOST_CHECK_EQUAL(ret1.d2, 27.5);
  BOOST_CHECK_EQUAL(ret1.d3, "test3");
  BOOST_REQUIRE_EXCEPTION(
    sub2_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });

  Data2 sent_t4{ 58, 28.5, "test4" };
  pub1_sender->send_with_topic(std::move(sent_t4), dunedaq::iomanager::Sender::s_no_block, "sub2_topic");
  BOOST_REQUIRE_EXCEPTION(
    sub1_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(
    sub2_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });

  Data2 sent_t5{ 59, 29.5, "test5" };
  pub2_sender->send_with_topic(std::move(sent_t5), dunedaq::iomanager::Sender::s_no_block, "sub2_topic");
  BOOST_REQUIRE_EXCEPTION(
    sub1_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  auto ret2 = sub2_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret2.d1, 59);
  BOOST_CHECK_EQUAL(ret2.d2, 29.5);
  BOOST_CHECK_EQUAL(ret2.d3, "test5");
}

BOOST_FIXTURE_TEST_CASE(NonSerializableSendReceive, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonSerializableData>(conn_id);
  auto net_receiver = IOManager::get()->get_receiver<NonSerializableData>(conn_id);
  auto q_sender = IOManager::get()->get_sender<NonSerializableData>(queue_id);
  auto q_receiver = IOManager::get()->get_receiver<NonSerializableData>(queue_id);

  NonSerializableData sent_nw(56, 26.5, "test1");
  NonSerializableData sent_q(57, 27.5, "test2");
  BOOST_REQUIRE_EXCEPTION(net_sender->send(std::move(sent_nw), dunedaq::iomanager::Sender::s_no_block),
                          NetworkMessageNotSerializable,
                          [](NetworkMessageNotSerializable const&) { return true; });

  NonSerializableData ret;
  BOOST_REQUIRE_EXCEPTION(ret = net_receiver->receive(std::chrono::milliseconds(10)),
                          NetworkMessageNotSerializable,
                          [](NetworkMessageNotSerializable const&) { return true; });

  q_sender->send(std::move(sent_q), std::chrono::milliseconds(10));

  ret = q_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 57);
  BOOST_CHECK_EQUAL(ret.d2, 27.5);
  BOOST_CHECK_EQUAL(ret.d3, "test2");
}

BOOST_FIXTURE_TEST_CASE(NonCopyableSendReceive, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonCopyableData>(conn_id);
  auto net_receiver = IOManager::get()->get_receiver<NonCopyableData>(conn_id);
  auto q_sender = IOManager::get()->get_sender<NonCopyableData>(queue_id);
  auto q_receiver = IOManager::get()->get_receiver<NonCopyableData>(queue_id);

  NonCopyableData sent_nw(56, 26.5, "test1");
  NonCopyableData sent_q(57, 27.5, "test2");
  net_sender->send(std::move(sent_nw), dunedaq::iomanager::Sender::s_no_block);

  auto ret = net_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 56);
  BOOST_CHECK_EQUAL(ret.d2, 26.5);
  BOOST_CHECK_EQUAL(ret.d3, "test1");

  q_sender->send(std::move(sent_q), std::chrono::milliseconds(10));

  ret = q_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 57);
  BOOST_CHECK_EQUAL(ret.d2, 27.5);
  BOOST_CHECK_EQUAL(ret.d3, "test2");
}

BOOST_FIXTURE_TEST_CASE(NonSerializableNonCopyableSendReceive, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonSerializableNonCopyable>(conn_id);
  auto net_receiver = IOManager::get()->get_receiver<NonSerializableNonCopyable>(conn_id);
  auto q_sender = IOManager::get()->get_sender<NonSerializableNonCopyable>(queue_id);
  auto q_receiver = IOManager::get()->get_receiver<NonSerializableNonCopyable>(queue_id);

  NonSerializableNonCopyable sent_nw(56, 26.5, "test1");
  NonSerializableNonCopyable sent_q(57, 27.5, "test2");
  BOOST_REQUIRE_EXCEPTION(net_sender->send(std::move(sent_nw), dunedaq::iomanager::Sender::s_no_block),
                          NetworkMessageNotSerializable,
                          [](NetworkMessageNotSerializable const&) { return true; });

  NonSerializableNonCopyable ret;
  BOOST_REQUIRE_EXCEPTION(ret = net_receiver->receive(std::chrono::milliseconds(10)),
                          NetworkMessageNotSerializable,
                          [](NetworkMessageNotSerializable const&) { return true; });

  q_sender->send(std::move(sent_q), std::chrono::milliseconds(10));

  ret = q_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret.d1, 57);
  BOOST_CHECK_EQUAL(ret.d2, 27.5);
  BOOST_CHECK_EQUAL(ret.d3, "test2");
}

BOOST_FIXTURE_TEST_CASE(CallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<Data>(conn_id);
  auto q_sender = IOManager::get()->get_sender<Data>(queue_id);

  Data sent_data_nw(56, 26.5, "test1");
  Data sent_data_q(57, 27.5, "test2");
  Data recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(Data&)> callback = [&](Data& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  IOManager::get()->add_callback<Data>(conn_id, callback);
  IOManager::get()->add_callback<Data>(queue_id, callback);

  usleep(1000);

  net_sender->send(std::move(sent_data_nw), dunedaq::iomanager::Sender::s_no_block);

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 56);
  BOOST_CHECK_EQUAL(recv_data.d2, 26.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test1");

  has_received_data = false;
  q_sender->send(std::move(sent_data_q), std::chrono::milliseconds(10));

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 57);
  BOOST_CHECK_EQUAL(recv_data.d2, 27.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test2");

  IOManager::get()->remove_callback<Data>(conn_id);
  IOManager::get()->remove_callback<Data>(queue_id);
}

BOOST_FIXTURE_TEST_CASE(NonCopyableCallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonCopyableData>(conn_id);
  auto q_sender = IOManager::get()->get_sender<NonCopyableData>(queue_id);

  NonCopyableData sent_data_nw(56, 26.5, "test1");
  NonCopyableData sent_data_q(57, 27.5, "test2");
  NonCopyableData recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(NonCopyableData&)> callback = [&](NonCopyableData& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  IOManager::get()->add_callback<NonCopyableData>(conn_id, callback);
  IOManager::get()->add_callback<NonCopyableData>(queue_id, callback);

  usleep(1000);

  net_sender->send(std::move(sent_data_nw), dunedaq::iomanager::Sender::s_no_block);

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 56);
  BOOST_CHECK_EQUAL(recv_data.d2, 26.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test1");

  has_received_data = false;
  q_sender->send(std::move(sent_data_q), std::chrono::milliseconds(10));

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 57);
  BOOST_CHECK_EQUAL(recv_data.d2, 27.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test2");

  IOManager::get()->remove_callback<NonCopyableData>(conn_id);
  IOManager::get()->remove_callback<NonCopyableData>(queue_id);
}

BOOST_FIXTURE_TEST_CASE(NonSerializableCallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonSerializableData>(conn_id);
  auto q_sender = IOManager::get()->get_sender<NonSerializableData>(queue_id);

  NonSerializableData sent_data_nw(56, 26.5, "test1");
  NonSerializableData sent_data_q(57, 27.5, "test2");
  NonSerializableData recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(NonSerializableData&)> callback = [&](NonSerializableData& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  BOOST_REQUIRE_EXCEPTION(IOManager::get()->add_callback<NonSerializableData>(conn_id, callback),
                          NetworkMessageNotSerializable,
                          [](NetworkMessageNotSerializable const&) { return true; });
  IOManager::get()->add_callback<NonSerializableData>(queue_id, callback);

  // Have to stop the callback from endlessly setting recv_data to default-constructed object
  IOManager::get()->remove_callback<NonSerializableData>(conn_id);
  has_received_data = false;
  q_sender->send(std::move(sent_data_q), std::chrono::milliseconds(10));

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 57);
  BOOST_CHECK_EQUAL(recv_data.d2, 27.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test2");

  IOManager::get()->remove_callback<NonSerializableData>(queue_id);
}

BOOST_FIXTURE_TEST_CASE(NonSerializableNonCopyableCallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonSerializableNonCopyable>(conn_id);
  auto q_sender = IOManager::get()->get_sender<NonSerializableNonCopyable>(queue_id);

  NonSerializableNonCopyable sent_data_nw(56, 26.5, "test1");
  NonSerializableNonCopyable sent_data_q(57, 27.5, "test2");
  NonSerializableNonCopyable recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(NonSerializableNonCopyable&)> callback = [&](NonSerializableNonCopyable& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  BOOST_REQUIRE_EXCEPTION(IOManager::get()->add_callback<NonSerializableNonCopyable>(conn_id, callback),
                          NetworkMessageNotSerializable,
                          [](NetworkMessageNotSerializable const&) { return true; });
  IOManager::get()->add_callback<NonSerializableNonCopyable>(queue_id, callback);

  usleep(1000);

  // Have to stop the callback from endlessly setting recv_data to default-constructed object
  IOManager::get()->remove_callback<NonSerializableNonCopyable>(conn_id);
  has_received_data = false;
  q_sender->send(std::move(sent_data_q), std::chrono::milliseconds(10));

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 57);
  BOOST_CHECK_EQUAL(recv_data.d2, 27.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test2");

  IOManager::get()->remove_callback<NonSerializableNonCopyable>(queue_id);
}

// TODO: Eric Flumerfelt <eflumerf@github.com>, June-16-2022: Reimplement this test for IOManager
/*
BOOST_FIXTURE_TEST_CASE(SendThreadSafety, NetworkManagerTestFixture)
{
  TLOG_DEBUG(12) << "SendThreadSafety test case BEGIN";

  auto substr_proc = [&](int idx) {
    const std::string pattern_string =
      "aaaaabbbbbcccccdddddeeeeefffffggggghhhhhiiiiijjjjjkkkkklllllmmmmmnnnnnooooopppppqqqqqrrrrrssssstttttuuuuuvvvvvww"
      "wwwxxxxxyyyyyzzzzzAAAAABBBBBCCCCCDDDDDEEEEEFFFFFGGGGGHHHHHIIIIIJJJJJKKKKKLLLLLMMMMMNNNNNOOOOOPPPPPQQQQQRRRRRSSSS"
      "STTTTTUUUUUVVVVVWWWWWXXXXXYYYYYZZZZZ";
    auto string_idx = idx % pattern_string.size();
    if (string_idx + 5 < pattern_string.size()) {
      return pattern_string.substr(string_idx, 5);
    } else {
      return pattern_string.substr(string_idx, 5) + pattern_string.substr(0, string_idx - pattern_string.size() + 5);
    }
  };

  auto send_proc = [&](int idx) {
    std::string buf = std::to_string(idx) + substr_proc(idx);
    TLOG_DEBUG(10) << "Sending " << buf << " for idx " << idx;
    NetworkManager::get().send_to("foo", buf.c_str(), buf.size(), dunedaq::ipm::Sender::s_block);
  };

  auto recv_proc = [&](dunedaq::ipm::Receiver::Response response) {
    BOOST_REQUIRE(response.data.size() > 0);
    auto received_idx = std::stoi(std::string(response.data.begin(), response.data.end()));
    auto idx_string = std::to_string(received_idx);
    auto received_string = std::string(response.data.begin() + idx_string.size(), response.data.end());

    TLOG_DEBUG(11) << "Received " << received_string << " for idx " << received_idx;

    BOOST_REQUIRE_EQUAL(received_string.size(), 5);

    std::string check = substr_proc(received_idx);

    BOOST_REQUIRE_EQUAL(received_string, check);
  };

  NetworkManager::get().start_listening("foo");

  NetworkManager::get().register_callback("foo", recv_proc);

  const int thread_count = 1000;
  std::array<std::thread, thread_count> threads;

  TLOG_DEBUG(12) << "Before starting send threads";
  for (int idx = 0; idx < thread_count; ++idx) {
    threads[idx] = std::thread(send_proc, idx);
  }
  TLOG_DEBUG(12) << "After starting send threads";
  for (int idx = 0; idx < thread_count; ++idx) {
    threads[idx].join();
  }
  TLOG_DEBUG(12) << "SendThreadSafety test case END";
}
*/

// TODO: Eric Flumerfelt <eflumerf@github.com>, June-16-2022: Reimplement this test for IOManager
/*
BOOST_FIXTURE_TEST_CASE(OneListenerThreaded, NetworkManagerTestFixture)
{
  auto callback = [&](dunedaq::ipm::Receiver::Response) { return; };
  const int thread_count = 1000;
  std::atomic<size_t> num_connected = 0;
  std::atomic<size_t> num_fail = 0;

  auto reg_proc = [&](int idx) {
    try {
      NetworkManager::get().start_listening("foo");
      NetworkManager::get().register_callback("foo", callback);
    } catch (ListenerAlreadyRegistered const&) {
      num_fail++;
      TLOG_DEBUG(13) << "Listener " << idx << " failed to register";
      return;
    }
    TLOG_DEBUG(13) << "Listener " << idx << " successfully started";
    num_connected++;
  };

  std::array<std::thread, thread_count> threads;

  for (int idx = 0; idx < thread_count; ++idx) {
    threads[idx] = std::thread(reg_proc, idx);
  }
  for (int idx = 0; idx < thread_count; ++idx) {
    threads[idx].join();
  }

  BOOST_REQUIRE_EQUAL(num_connected.load(), 1);
  BOOST_REQUIRE_EQUAL(num_fail.load(), thread_count - 1);
}
*/

// TODO: Eric Flumerfelt <eflumerf@github.com>, June-16-2022: Reimplement this test for IOManager
/*
BOOST_AUTO_TEST_CASE(ManyThreadsSendingAndReceiving)
{
  const int num_sending_threads = 100;
  const int num_receivers = 50;

  nwmgr::Connections testConfig;
  for (int i = 0; i < num_receivers; ++i) {
    nwmgr::Connection testConn;
    testConn.name = "foo" + std::to_string(i);
    testConn.address = "inproc://bar" + std::to_string(i);
    testConfig.push_back(testConn);
  }
  NetworkManager::get().configure(testConfig);

  auto substr_proc = [](int idx) {
    const std::string pattern_string =
      "aaaaabbbbbcccccdddddeeeeefffffggggghhhhhiiiiijjjjjkkkkklllllmmmmmnnnnnooooopppppqqqqqrrrrrssssstttttuuuuuvvvvvww"
      "wwwxxxxxyyyyyzzzzzAAAAABBBBBCCCCCDDDDDEEEEEFFFFFGGGGGHHHHHIIIIIJJJJJKKKKKLLLLLMMMMMNNNNNOOOOOPPPPPQQQQQRRRRRSSSS"
      "STTTTTUUUUUVVVVVWWWWWXXXXXYYYYYZZZZZ";

    auto string_idx = idx % pattern_string.size();
    if (string_idx + 5 < pattern_string.size()) {
      return pattern_string.substr(string_idx, 5);
    } else {
      return pattern_string.substr(string_idx, 5) + pattern_string.substr(0, string_idx - pattern_string.size() + 5);
    }
  };
  auto send_proc = [&](int idx) {
    std::string buf = std::to_string(idx) + substr_proc(idx);
    for (int i = 0; i < num_receivers; ++i) {
      TLOG_DEBUG(14) << "Sending " << buf << " for idx " << idx << " to receiver " << i;
      NetworkManager::get().send_to("foo" + std::to_string(i), buf.c_str(), buf.size(), dunedaq::ipm::Sender::s_block);
    }
  };

  std::array<std::atomic<size_t>, num_receivers> messages_received;
  std::array<std::atomic<size_t>, num_receivers> num_empty_responses;
  std::array<std::atomic<size_t>, num_receivers> num_size_errors;
  std::array<std::atomic<size_t>, num_receivers> num_content_errors;
  std::array<std::function<void(dunedaq::ipm::Receiver::Response)>, num_receivers> recv_procs;

  for (int i = 0; i < num_receivers; ++i) {
    messages_received[i] = 0;
    num_empty_responses[i] = 0;
    num_size_errors[i] = 0;
    num_content_errors[i] = 0;
    recv_procs[i] = [&, i](dunedaq::ipm::Receiver::Response response) {
      if (response.data.size() == 0) {
        num_empty_responses[i]++;
      }
      auto received_idx = std::stoi(std::string(response.data.begin(), response.data.end()));
      auto idx_string = std::to_string(received_idx);
      auto received_string = std::string(response.data.begin() + idx_string.size(), response.data.end());

      TLOG_DEBUG(14) << "Receiver " << i << " received " << received_string << " for idx " << received_idx;

      if (received_string.size() != 5) {
        num_size_errors[i]++;
      }

      std::string check = substr_proc(received_idx);

      if (received_string != check) {
        num_content_errors[i]++;
      }
      messages_received[i]++;
    };
    NetworkManager::get().start_listening("foo" + std::to_string(i));
    NetworkManager::get().register_callback("foo" + std::to_string(i), recv_procs[i]);
  }

  std::array<std::thread, num_sending_threads> threads;

  TLOG_DEBUG(14) << "Before starting send threads";
  for (int idx = 0; idx < num_sending_threads; ++idx) {
    threads[idx] = std::thread(send_proc, idx);
  }
  TLOG_DEBUG(14) << "After starting send threads";
  for (int idx = 0; idx < num_sending_threads; ++idx) {
    threads[idx].join();
  }

  TLOG_DEBUG(14) << "Sleeping to allow all messages to be processed";
  std::this_thread::sleep_for(std::chrono::seconds(1));

  for (auto i = 0; i < num_receivers; ++i) {
    TLOG_DEBUG(14) << "Shutting down receiver " << i;
    NetworkManager::get().stop_listening("foo" + std::to_string(i));
    BOOST_CHECK_EQUAL(messages_received[i], num_sending_threads);
    BOOST_REQUIRE_EQUAL(num_empty_responses[i], 0);
    BOOST_REQUIRE_EQUAL(num_size_errors[i], 0);
    BOOST_REQUIRE_EQUAL(num_content_errors[i], 0);
  }

  TLOG_DEBUG(14) << "Resetting NetworkManager";
  NetworkManager::get().reset();
}
*/

BOOST_AUTO_TEST_SUITE_END()
