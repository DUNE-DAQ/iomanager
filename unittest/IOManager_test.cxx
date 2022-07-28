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
    setenv("DUNEDAQ_PARTITION", "IOManager_t", 0);

    conn_ep_s = Endpoint{ "data_t", "IOManager_test", "network_s", {}, Direction::kOutput };
    conn_ep_r = Endpoint{ "data_t", "IOManager_test", "network_r", {}, Direction::kInput };
    queue_ep = Endpoint{ "data_t", " IOManager_test ", "queue", {}, Direction::kUnspecified };
    pub1_ep = Endpoint{ "data2_t", "IOManager_test", "pub1", {}, Direction::kOutput };
    pub2_ep = Endpoint{ "data2_t", "IOManager_test", "pub2", {}, Direction::kOutput };
    pub3_ep = Endpoint{ "data3_t", "IOManager_test", "pub3", {}, Direction::kOutput };
    sub1_ep = Endpoint{ "data2_t", "IOManager_test", "*", {}, Direction::kInput };
    sub2_ep = Endpoint{ "data2_t", "IOManager_test", "pub2", {}, Direction::kInput };
    sub3_ep = Endpoint{ "data3_t", "IOManager_test", "*", {}, Direction::kInput };

    dunedaq::iomanager::Queues_t queues;
    queues.emplace_back(QueueConfig{ "test_queue", { queue_ep }, QueueType::kFollySPSCQueue, 50 });

    dunedaq::iomanager::Connections_t connections;
    connections.emplace_back(Connection{ conn_ep_r, { conn_ep_s }, "inproc://foo", ConnectionType::kSendRecv });
    connections.emplace_back(Connection{ pub1_ep, {}, "inproc://bar", ConnectionType::kPubSub });
    connections.emplace_back(Connection{ pub2_ep, {}, "inproc://baz", ConnectionType::kPubSub });
    connections.emplace_back(Connection{ pub3_ep, {}, "inproc://qui", ConnectionType::kPubSub });
    IOManager::get()->configure(queues, connections);
  }
  ~ConfigurationTestFixture() { IOManager::get()->reset(); }

  ConfigurationTestFixture(ConfigurationTestFixture const&) = default;
  ConfigurationTestFixture(ConfigurationTestFixture&&) = default;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture const&) = default;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture&&) = default;

  Endpoint conn_ep_r;
  Endpoint conn_ep_s;
  Endpoint queue_ep;
  Endpoint pub1_ep;
  Endpoint pub2_ep;
  Endpoint pub3_ep;
  Endpoint sub1_ep;
  Endpoint sub2_ep;
  Endpoint sub3_ep;
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
  setenv("DUNEDAQ_PARTITION", "IOManager_t", 0);

  Endpoint output_ep = Endpoint{ "data_t", "IOManager_test", "output", {}, Direction::kOutput };
  Endpoint input_ep = Endpoint{ "data_t", "IOManager_test", "input", {}, Direction::kInput };
  Endpoint unspecified_ep = Endpoint{ "data_t", "IOManager_test", "unspecified", {}, Direction::kUnspecified };

  dunedaq::iomanager::Queues_t queues;
  queues.emplace_back(QueueConfig{ "test_queue", { unspecified_ep }, QueueType::kFollySPSCQueue, 50 });

  dunedaq::iomanager::Connections_t connections;
  connections.emplace_back(Connection{ input_ep, { output_ep }, "inproc://foo", ConnectionType::kSendRecv });
  IOManager::get()->configure(queues, connections);

  // Unspecified is always ok
  auto sender = IOManager::get()->get_sender<Data>(unspecified_ep);
  auto receiver = IOManager::get()->get_receiver<Data>(unspecified_ep);

  // Input can only be receiver
  BOOST_REQUIRE_EXCEPTION(IOManager::get()->get_sender<Data>(input_ep),
                          ConnectionDirectionMismatch,
                          [](ConnectionDirectionMismatch const&) { return true; });
  receiver = IOManager::get()->get_receiver<Data>(input_ep);

  // Output can only be sender
  sender = IOManager::get()->get_sender<Data>(output_ep);
  BOOST_REQUIRE_EXCEPTION(IOManager::get()->get_receiver<Data>(output_ep),
                          ConnectionDirectionMismatch,
                          [](ConnectionDirectionMismatch const&) { return true; });
  IOManager::get()->reset();
}

BOOST_FIXTURE_TEST_CASE(SimpleSendReceive, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<Data>(conn_ep_s);
  auto net_receiver = IOManager::get()->get_receiver<Data>(conn_ep_r);
  auto q_sender = IOManager::get()->get_sender<Data>(queue_ep);
  auto q_receiver = IOManager::get()->get_receiver<Data>(queue_ep);

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
  auto pub1_sender = IOManager::get()->get_sender<Data>(pub1_ep);
  auto pub2_sender = IOManager::get()->get_sender<Data>(pub2_ep);
  auto pub3_sender = IOManager::get()->get_sender<Data>(pub3_ep);
  auto sub1_receiver = IOManager::get()->get_receiver<Data>(sub1_ep);
  auto sub2_receiver = IOManager::get()->get_receiver<Data>(sub2_ep);
  auto sub3_receiver = IOManager::get()->get_receiver<Data>(sub3_ep);

  // Sub1 is subscribed to all data_t publishers, Sub2 only to pub2, Sub3 to all data2_t
  Data sent_t1(56, 26.5, "test1");
  pub1_sender->send(std::move(sent_t1), dunedaq::iomanager::Sender::s_no_block);

  auto ret1 = sub1_receiver->receive(std::chrono::milliseconds(10));
  BOOST_REQUIRE_EXCEPTION(
    sub2_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_REQUIRE_EXCEPTION(
    sub3_receiver->receive(std::chrono::milliseconds(10)), TimeoutExpired, [](TimeoutExpired const&) { return true; });
  BOOST_CHECK_EQUAL(ret1.d1, 56);
  BOOST_CHECK_EQUAL(ret1.d2, 26.5);
  BOOST_CHECK_EQUAL(ret1.d3, "test1");

  Data sent_t2(57, 27.5, "test2");
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

  Data sent_t3(58, 28.5, "test3");
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

BOOST_FIXTURE_TEST_CASE(NonSerializableSendReceive, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonSerializableData>(conn_ep_s);
  auto net_receiver = IOManager::get()->get_receiver<NonSerializableData>(conn_ep_r);
  auto q_sender = IOManager::get()->get_sender<NonSerializableData>(queue_ep);
  auto q_receiver = IOManager::get()->get_receiver<NonSerializableData>(queue_ep);

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
  auto net_sender = IOManager::get()->get_sender<NonCopyableData>(conn_ep_s);
  auto net_receiver = IOManager::get()->get_receiver<NonCopyableData>(conn_ep_r);
  auto q_sender = IOManager::get()->get_sender<NonCopyableData>(queue_ep);
  auto q_receiver = IOManager::get()->get_receiver<NonCopyableData>(queue_ep);

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
  auto net_sender = IOManager::get()->get_sender<NonSerializableNonCopyable>(conn_ep_s);
  auto net_receiver = IOManager::get()->get_receiver<NonSerializableNonCopyable>(conn_ep_r);
  auto q_sender = IOManager::get()->get_sender<NonSerializableNonCopyable>(queue_ep);
  auto q_receiver = IOManager::get()->get_receiver<NonSerializableNonCopyable>(queue_ep);

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
  auto net_sender = IOManager::get()->get_sender<Data>(conn_ep_s);
  auto q_sender = IOManager::get()->get_sender<Data>(queue_ep);

  Data sent_data_nw(56, 26.5, "test1");
  Data sent_data_q(57, 27.5, "test2");
  Data recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(Data&)> callback = [&](Data& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  IOManager::get()->add_callback<Data>(conn_ep_r, callback);
  IOManager::get()->add_callback<Data>(queue_ep, callback);

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

  IOManager::get()->remove_callback<Data>(conn_ep_r);
  IOManager::get()->remove_callback<Data>(queue_ep);
}

BOOST_FIXTURE_TEST_CASE(NonCopyableCallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonCopyableData>(conn_ep_s);
  auto q_sender = IOManager::get()->get_sender<NonCopyableData>(queue_ep);

  NonCopyableData sent_data_nw(56, 26.5, "test1");
  NonCopyableData sent_data_q(57, 27.5, "test2");
  NonCopyableData recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(NonCopyableData&)> callback = [&](NonCopyableData& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  IOManager::get()->add_callback<NonCopyableData>(conn_ep_r, callback);
  IOManager::get()->add_callback<NonCopyableData>(queue_ep, callback);

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

  IOManager::get()->remove_callback<NonCopyableData>(conn_ep_r);
  IOManager::get()->remove_callback<NonCopyableData>(queue_ep);
}

BOOST_FIXTURE_TEST_CASE(NonSerializableCallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonSerializableData>(conn_ep_s);
  auto q_sender = IOManager::get()->get_sender<NonSerializableData>(queue_ep);

  NonSerializableData sent_data_nw(56, 26.5, "test1");
  NonSerializableData sent_data_q(57, 27.5, "test2");
  NonSerializableData recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(NonSerializableData&)> callback = [&](NonSerializableData& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  BOOST_REQUIRE_EXCEPTION(IOManager::get()->add_callback<NonSerializableData>(conn_ep_r, callback),
                          NetworkMessageNotSerializable,
                          [](NetworkMessageNotSerializable const&) { return true; });
  IOManager::get()->add_callback<NonSerializableData>(queue_ep, callback);

  // Have to stop the callback from endlessly setting recv_data to default-constructed object
  IOManager::get()->remove_callback<NonSerializableData>(conn_ep_r);
  has_received_data = false;
  q_sender->send(std::move(sent_data_q), std::chrono::milliseconds(10));

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 57);
  BOOST_CHECK_EQUAL(recv_data.d2, 27.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test2");

  IOManager::get()->remove_callback<NonSerializableData>(queue_ep);
}

BOOST_FIXTURE_TEST_CASE(NonSerializableNonCopyableCallbackRegistration, ConfigurationTestFixture)
{
  auto net_sender = IOManager::get()->get_sender<NonSerializableNonCopyable>(conn_ep_s);
  auto q_sender = IOManager::get()->get_sender<NonSerializableNonCopyable>(queue_ep);

  NonSerializableNonCopyable sent_data_nw(56, 26.5, "test1");
  NonSerializableNonCopyable sent_data_q(57, 27.5, "test2");
  NonSerializableNonCopyable recv_data;
  std::atomic<bool> has_received_data = false;

  std::function<void(NonSerializableNonCopyable&)> callback = [&](NonSerializableNonCopyable& d) {
    has_received_data = true;
    recv_data = std::move(d);
  };

  BOOST_REQUIRE_EXCEPTION(IOManager::get()->add_callback<NonSerializableNonCopyable>(conn_ep_r, callback),
                          NetworkMessageNotSerializable,
                          [](NetworkMessageNotSerializable const&) { return true; });
  IOManager::get()->add_callback<NonSerializableNonCopyable>(queue_ep, callback);

  usleep(1000);

  // Have to stop the callback from endlessly setting recv_data to default-constructed object
  IOManager::get()->remove_callback<NonSerializableNonCopyable>(conn_ep_r);
  has_received_data = false;
  q_sender->send(std::move(sent_data_q), std::chrono::milliseconds(10));

  while (!has_received_data.load())
    usleep(1000);

  BOOST_CHECK_EQUAL(recv_data.d1, 57);
  BOOST_CHECK_EQUAL(recv_data.d2, 27.5);
  BOOST_CHECK_EQUAL(recv_data.d3, "test2");

  IOManager::get()->remove_callback<NonSerializableNonCopyable>(queue_ep);
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
