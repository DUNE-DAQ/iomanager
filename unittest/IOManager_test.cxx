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
    Connections_t connections;
    connections.emplace_back(
      Connection{ "test_queue", ServiceType::kQueue, "NonCopyableData", "queue://StdDeQueue:10" });
    connections.emplace_back(
      Connection{ "test_connection_r", ServiceType::kNetReceiver, "NonCopyableData", "inproc://foo" });
    connections.emplace_back(
      Connection{ "test_connection_s", ServiceType::kNetSender, "NonCopyableData", "inproc://foo" });
    connections.emplace_back(Connection{ "test_pubsub_connection_p",
                                           ServiceType::kPublisher,
                                           "NonCopyableData",
                                           "inproc://bar",
                                           { "test_topic", "another_test_topic" } });
    connections.emplace_back(Connection{ "test_pubsub_connection_s",
                                           ServiceType::kSubscriber,
                                           "NonCopyableData",
                                           "inproc://qui",
                                           { "test_topic", "another_test_topic" } });
    connections.emplace_back(
      Connection{ "test_pubsub_connection_s2", ServiceType::kSubscriber, "NonCopyableData", "inproc://bar", { "" } });
    connections.emplace_back(Connection{ "another_test_pubsub_connection_p",
                                           ServiceType::kPublisher,
                                           "NonCopyableData",
                                           "inproc://baz",
                                           { "another_test_topic" } });
    connections.emplace_back(Connection{ "another_test_pubsub_connection_s",
                                           ServiceType::kSubscriber,
                                           "NonCopyableData",
                                           "inproc://qua",
                                           { "another_test_topic" } });
    IOManager::get()->configure(connections);
    conn_ref_r = Endpoint{ "network_r", "test_connection_r" };
    conn_ref_s = Endpoint{ "network_s", "test_connection_s" };
    queue_ref = Endpoint{ "queue", "test_queue" };
    pub1_ref = Endpoint{ "pub1", "test_pubsub_connection_p", Direction::kOutput };
    pub2_ref = Endpoint{ "pub2", "another_test_pubsub_connection_p", Direction::kOutput };
    sub1_ref = Endpoint{ "sub1", "test_topic", Direction::kInput };
    sub2_ref = Endpoint{ "sub2", "another_test_topic", Direction::kInput };
    sub3_ref = Endpoint{ "sub3", "test_pubsub_connection_s2", Direction::kInput };
  }
  ~ConfigurationTestFixture() { IOManager::get()->reset(); }

  ConfigurationTestFixture(ConfigurationTestFixture const&) = default;
  ConfigurationTestFixture(ConfigurationTestFixture&&) = default;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture const&) = default;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture&&) = default;

  Endpoint conn_ref_r;
  Endpoint conn_ref_s;
  Endpoint queue_ref;
  Endpoint pub1_ref;
  Endpoint pub2_ref;
  Endpoint sub1_ref;
  Endpoint sub2_ref;
  Endpoint sub3_ref;
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

BOOST_AUTO_TEST_CASE(Directionality)
{
  Connections_t connections;
  connections.emplace_back(Connection{ "test_connection_r", ServiceType::kNetReceiver, "Data", "inproc://foo" });
  connections.emplace_back(Connection{ "test_connection_s", ServiceType::kNetSender, "Data", "inproc://foo" });
  IOManager::get()->configure(connections);
  Endpoint unspecified_ref_r = Endpoint{ "unspecified_r", "test_connection_r" };
  Endpoint unspecified_ref_s = Endpoint{ "unspecified_s", "test_connection_s" };
  Endpoint input_ref_r = Endpoint{ "input_r", "test_connection_r", Direction::kInput };
  Endpoint output_ref_r = Endpoint{ "output_r", "test_connection_r", Direction::kOutput };
  Endpoint input_ref_s = Endpoint{ "input_s", "test_connection_s", Direction::kInput };
  Endpoint output_ref_s = Endpoint{ "output_s", "test_connection_s", Direction::kOutput };

  // Unspecified is always ok
  auto sender = IOManager::get()->get_sender<Data>(unspecified_ref_s);
  auto receiver = IOManager::get()->get_receiver<Data>(unspecified_ref_r);

  // But mismatching service type is not
  BOOST_REQUIRE_EXCEPTION(IOManager::get()->get_sender<Data>(unspecified_ref_r),
                          ConnectionDirectionMismatch,
                          [](ConnectionDirectionMismatch const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(IOManager::get()->get_receiver<Data>(unspecified_ref_s),
                          ConnectionDirectionMismatch,
                          [](ConnectionDirectionMismatch const&) { return true; });

  // Input can only be receiver
  BOOST_REQUIRE_EXCEPTION(IOManager::get()->get_sender<Data>(input_ref_s),
                          ConnectionDirectionMismatch,
                          [](ConnectionDirectionMismatch const&) { return true; });
  receiver = IOManager::get()->get_receiver<Data>(input_ref_r);

  // Output can only be sender
  sender = IOManager::get()->get_sender<Data>(output_ref_s);
  BOOST_REQUIRE_EXCEPTION(IOManager::get()->get_receiver<Data>(output_ref_r),
                          ConnectionDirectionMismatch,
                          [](ConnectionDirectionMismatch const&) { return true; });

  // Receiver can only be input
  BOOST_REQUIRE_EXCEPTION(IOManager::get()->get_sender<Data>(input_ref_r),
                          ConnectionDirectionMismatch,
                          [](ConnectionDirectionMismatch const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(IOManager::get()->get_receiver<Data>(input_ref_s),
                          ConnectionDirectionMismatch,
                          [](ConnectionDirectionMismatch const&) { return true; });

  // Sender can only be output
  BOOST_REQUIRE_EXCEPTION(IOManager::get()->get_sender<Data>(output_ref_r),
                          ConnectionDirectionMismatch,
                          [](ConnectionDirectionMismatch const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(IOManager::get()->get_receiver<Data>(output_ref_s),
                          ConnectionDirectionMismatch,
                          [](ConnectionDirectionMismatch const&) { return true; });

  IOManager::get()->reset();
}

BOOST_FIXTURE_TEST_CASE(SimpleSendReceive, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<Data>(conn_ref_s);
  auto net_receiver = IOManager::get()->get_receiver<Data>(conn_ref_r);
  auto q_sender = IOManager::get()->get_sender<Data>(queue_ref);
  auto q_receiver = IOManager::get()->get_receiver<Data>(queue_ref);

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

BOOST_FIXTURE_TEST_CASE(GetByName, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<Data>("test_connection_s");
  auto net_receiver = IOManager::get()->get_receiver<Data>("test_connection_r");
  auto q_sender = IOManager::get()->get_sender<Data>("test_queue");
  auto q_receiver = IOManager::get()->get_receiver<Data>("test_queue");

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
  auto pub1_sender = IOManager::get()->get_sender<Data>(pub1_ref);
  auto pub2_sender = IOManager::get()->get_sender<Data>(pub2_ref);
  auto sub1_receiver = IOManager::get()->get_receiver<Data>(sub1_ref);
  auto sub2_receiver = IOManager::get()->get_receiver<Data>(sub2_ref);
  auto sub3_receiver = IOManager::get()->get_receiver<Data>(sub3_ref);

  // Sub1 is subscribed to test_topic, Sub3 sees all messages sent by pub1
  Data sent_t1(56, 26.5, "test1");
  pub1_sender->send(std::move(sent_t1), dunedaq::iomanager::Sender::s_no_block, "test_topic");

  auto ret1 = sub1_receiver->receive(std::chrono::milliseconds(10));
  BOOST_REQUIRE_EXCEPTION(
    sub2_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  auto ret3 = sub3_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret1.d1, 56);
  BOOST_CHECK_EQUAL(ret1.d2, 26.5);
  BOOST_CHECK_EQUAL(ret1.d3, "test1");
  BOOST_CHECK_EQUAL(ret3.d1, 56);
  BOOST_CHECK_EQUAL(ret3.d2, 26.5);
  BOOST_CHECK_EQUAL(ret3.d3, "test1");

  // Sub2 is subscribed to another_test_topic, Sub3 sees all messages sent by pub1
  Data sent_t2(57, 27.5, "test2");
  pub1_sender->send(std::move(sent_t2), dunedaq::iomanager::Sender::s_no_block, "another_test_topic");

  BOOST_REQUIRE_EXCEPTION(
    sub1_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  auto ret2 = sub2_receiver->receive(std::chrono::milliseconds(10));
  ret3 = sub3_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret2.d1, 57);
  BOOST_CHECK_EQUAL(ret2.d2, 27.5);
  BOOST_CHECK_EQUAL(ret2.d3, "test2");
  BOOST_CHECK_EQUAL(ret3.d1, 57);
  BOOST_CHECK_EQUAL(ret3.d2, 27.5);
  BOOST_CHECK_EQUAL(ret3.d3, "test2");

  // Sub3 sees all messages sent by pub1
  Data sent_t3(58, 28.5, "test3");
  pub1_sender->send(std::move(sent_t3), dunedaq::iomanager::Sender::s_no_block, "invalid_topic");

  BOOST_REQUIRE_EXCEPTION(
    sub1_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(
    sub2_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  ret3 = sub3_receiver->receive(std::chrono::milliseconds(10));
  BOOST_CHECK_EQUAL(ret3.d1, 58);
  BOOST_CHECK_EQUAL(ret3.d2, 28.5);
  BOOST_CHECK_EQUAL(ret3.d3, "test3");

  // Nobody receives a message sent to test_topic by pub2 (not a registered topic on that connection)
  Data sent_t4(59, 29.5, "test4");
  pub2_sender->send(std::move(sent_t4), dunedaq::iomanager::Sender::s_no_block, "test_topic");

  BOOST_REQUIRE_EXCEPTION(
    sub1_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(
    sub2_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(
    sub3_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });

  // Sub2 is subscribed to another_test_topic
  Data sent_t5(60, 30.5, "test5");
  pub2_sender->send(std::move(sent_t5), dunedaq::iomanager::Sender::s_no_block, "another_test_topic");

  BOOST_REQUIRE_EXCEPTION(
    sub1_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  ret2 = sub2_receiver->receive(std::chrono::milliseconds(10));
  BOOST_REQUIRE_EXCEPTION(
    sub3_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_CHECK_EQUAL(ret2.d1, 60);
  BOOST_CHECK_EQUAL(ret2.d2, 30.5);
  BOOST_CHECK_EQUAL(ret2.d3, "test5");

  // Nobody receives a message sent to invalid_topic by pub2
  Data sent_t6(61, 31.5, "test6");
  pub2_sender->send(std::move(sent_t6), dunedaq::iomanager::Sender::s_no_block, "invalid_topic");

  BOOST_REQUIRE_EXCEPTION(
    sub1_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(
    sub2_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(
    sub3_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
}

BOOST_FIXTURE_TEST_CASE(NonSerializableSendReceive, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonSerializableData>(conn_ref_s);
  auto net_receiver = IOManager::get()->get_receiver<NonSerializableData>(conn_ref_r);
  auto q_sender = IOManager::get()->get_sender<NonSerializableData>(queue_ref);
  auto q_receiver = IOManager::get()->get_receiver<NonSerializableData>(queue_ref);

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
  auto net_sender = IOManager::get()->get_sender<NonCopyableData>(conn_ref_s);
  auto net_receiver = IOManager::get()->get_receiver<NonCopyableData>(conn_ref_r);
  auto q_sender = IOManager::get()->get_sender<NonCopyableData>(queue_ref);
  auto q_receiver = IOManager::get()->get_receiver<NonCopyableData>(queue_ref);

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
  auto net_sender = IOManager::get()->get_sender<NonSerializableNonCopyable>(conn_ref_s);
  auto net_receiver = IOManager::get()->get_receiver<NonSerializableNonCopyable>(conn_ref_r);
  auto q_sender = IOManager::get()->get_sender<NonSerializableNonCopyable>(queue_ref);
  auto q_receiver = IOManager::get()->get_receiver<NonSerializableNonCopyable>(queue_ref);

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
  auto net_sender = IOManager::get()->get_sender<Data>(conn_ref_s);
  auto q_sender = IOManager::get()->get_sender<Data>(queue_ref);

  Data sent_data_nw(56, 26.5, "test1");
  Data sent_data_q(57, 27.5, "test2");
  Data recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(Data&)> callback = [&](Data& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  IOManager::get()->add_callback<Data>(conn_ref_r, callback);
  IOManager::get()->add_callback<Data>(queue_ref, callback);

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

  IOManager::get()->remove_callback<Data>(conn_ref_r);
  IOManager::get()->remove_callback<Data>(queue_ref);
}

BOOST_FIXTURE_TEST_CASE(NonCopyableCallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonCopyableData>(conn_ref_s);
  auto q_sender = IOManager::get()->get_sender<NonCopyableData>(queue_ref);

  NonCopyableData sent_data_nw(56, 26.5, "test1");
  NonCopyableData sent_data_q(57, 27.5, "test2");
  NonCopyableData recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(NonCopyableData&)> callback = [&](NonCopyableData& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  IOManager::get()->add_callback<NonCopyableData>(conn_ref_r, callback);
  IOManager::get()->add_callback<NonCopyableData>(queue_ref, callback);

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

  IOManager::get()->remove_callback<NonCopyableData>(conn_ref_r);
  IOManager::get()->remove_callback<NonCopyableData>(queue_ref);
}

BOOST_FIXTURE_TEST_CASE(NonSerializableCallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonSerializableData>(conn_ref_s);
  auto q_sender = IOManager::get()->get_sender<NonSerializableData>(queue_ref);

  NonSerializableData sent_data_nw(56, 26.5, "test1");
  NonSerializableData sent_data_q(57, 27.5, "test2");
  NonSerializableData recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(NonSerializableData&)> callback = [&](NonSerializableData& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  BOOST_REQUIRE_EXCEPTION(IOManager::get()->add_callback<NonSerializableData>(conn_ref_r, callback),
                          NetworkMessageNotSerializable,
                          [](NetworkMessageNotSerializable const&) { return true; });
  IOManager::get()->add_callback<NonSerializableData>(queue_ref, callback);

  // Have to stop the callback from endlessly setting recv_data to default-constructed object
  IOManager::get()->remove_callback<NonSerializableData>(conn_ref_r);
  has_received_data = false;
  q_sender->send(std::move(sent_data_q), std::chrono::milliseconds(10));

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 57);
  BOOST_CHECK_EQUAL(recv_data.d2, 27.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test2");

  IOManager::get()->remove_callback<NonSerializableData>(queue_ref);
}

BOOST_FIXTURE_TEST_CASE(NonSerializableNonCopyableCallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonSerializableNonCopyable>(conn_ref_s);
  auto q_sender = IOManager::get()->get_sender<NonSerializableNonCopyable>(queue_ref);

  NonSerializableNonCopyable sent_data_nw(56, 26.5, "test1");
  NonSerializableNonCopyable sent_data_q(57, 27.5, "test2");
  NonSerializableNonCopyable recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(NonSerializableNonCopyable&)> callback = [&](NonSerializableNonCopyable& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  BOOST_REQUIRE_EXCEPTION(IOManager::get()->add_callback<NonSerializableNonCopyable>(conn_ref_r, callback),
                          NetworkMessageNotSerializable,
                          [](NetworkMessageNotSerializable const&) { return true; });
  IOManager::get()->add_callback<NonSerializableNonCopyable>(queue_ref, callback);

  usleep(1000);

  // Have to stop the callback from endlessly setting recv_data to default-constructed object
  IOManager::get()->remove_callback<NonSerializableNonCopyable>(conn_ref_r);
  has_received_data = false;
  q_sender->send(std::move(sent_data_q), std::chrono::milliseconds(10));

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 57);
  BOOST_CHECK_EQUAL(recv_data.d2, 27.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test2");

  IOManager::get()->remove_callback<NonSerializableNonCopyable>(queue_ref);
}

BOOST_AUTO_TEST_SUITE_END()
