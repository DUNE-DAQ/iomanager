/**
 * @file NetworkManager_test.cxx NetworkManager class Unit Tests
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/connection/Structs.hpp"
#include "iomanager/network/NetworkManager.hpp"

#include "logging/Logging.hpp"

#define BOOST_TEST_MODULE NetworkManager_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <string>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

using namespace dunedaq::iomanager;

BOOST_AUTO_TEST_SUITE(NetworkManager_test)

struct NetworkManagerTestFixture
{
  NetworkManagerTestFixture()
  {
    setenv("DUNEDAQ_PARTITION", "NetworkManager_t", 0);

    Connections_t testConfig;
    Connection testConn;

    bind_endpoint_sendrecv.data_type = "sendrecv";
    bind_endpoint_sendrecv.app_name = "NetworkManager_test";
    bind_endpoint_sendrecv.module_name = "Config";
    bind_endpoint_sendrecv.direction = Direction::kInput;

    bind_endpoint_pubsub.data_type = "pubsub";
    bind_endpoint_pubsub.app_name = "NetworkManager_test";
    bind_endpoint_pubsub.module_name = "Config";
    bind_endpoint_pubsub.direction = Direction::kOutput;

    bind_endpoint_different_pubsub.data_type = "different_pubsub";
    bind_endpoint_different_pubsub.app_name = "NetworkManager_test";
    bind_endpoint_different_pubsub.module_name = "Config";
    bind_endpoint_different_pubsub.direction = Direction::kOutput;

    testConn.bind_endpoint = bind_endpoint_sendrecv;
    testConn.uri = "inproc://foo";
    testConn.connection_type = ConnectionType::kSendRecv;
    testConfig.push_back(testConn);

    testConn.connection_type = ConnectionType::kPubSub;
    testConn.bind_endpoint = bind_endpoint_pubsub;
    testConn.uri = "inproc://bar";
    testConfig.push_back(testConn);

    testConn.bind_endpoint.module_name = "Config2";
    testConn.uri = "inproc://rab";
    testConfig.push_back(testConn);

    testConn.bind_endpoint = bind_endpoint_different_pubsub;
    testConn.uri = "inproc://abr";
    testConfig.push_back(testConn);
    NetworkManager::get().configure(testConfig);

    req_sendrecv = { bind_endpoint_sendrecv.data_type,
                     bind_endpoint_sendrecv.app_name,
                     bind_endpoint_sendrecv.module_name,
                     bind_endpoint_sendrecv.source_id };
    req_pubsub = { bind_endpoint_pubsub.data_type,
                   bind_endpoint_pubsub.app_name,
                   bind_endpoint_pubsub.module_name,
                   bind_endpoint_pubsub.source_id };
    req_different_pubsub = { bind_endpoint_different_pubsub.data_type,
                             bind_endpoint_different_pubsub.app_name,
                             bind_endpoint_different_pubsub.module_name,
                             bind_endpoint_different_pubsub.source_id };
  }
  ~NetworkManagerTestFixture() { NetworkManager::get().reset(); }

  NetworkManagerTestFixture(NetworkManagerTestFixture const&) = default;
  NetworkManagerTestFixture(NetworkManagerTestFixture&&) = default;
  NetworkManagerTestFixture& operator=(NetworkManagerTestFixture const&) = default;
  NetworkManagerTestFixture& operator=(NetworkManagerTestFixture&&) = default;

  Endpoint bind_endpoint_sendrecv, bind_endpoint_pubsub, bind_endpoint_different_pubsub;
  ConnectionRequest req_sendrecv, req_pubsub, req_different_pubsub;
};

BOOST_AUTO_TEST_CASE(CopyAndMoveSemantics)
{
  BOOST_REQUIRE(!std::is_copy_constructible_v<NetworkManager>);
  BOOST_REQUIRE(!std::is_copy_assignable_v<NetworkManager>);
  BOOST_REQUIRE(!std::is_move_constructible_v<NetworkManager>);
  BOOST_REQUIRE(!std::is_move_assignable_v<NetworkManager>);
}

BOOST_AUTO_TEST_CASE(Singleton)
{
  auto& nm = NetworkManager::get();
  auto& another_nm = NetworkManager::get();

  BOOST_REQUIRE_EQUAL(&nm, &another_nm);
}

BOOST_FIXTURE_TEST_CASE(FakeConfigure, NetworkManagerTestFixture)
{
  auto conn_res = NetworkManager::get().get_preconfigured_connections(req_sendrecv);
  BOOST_REQUIRE_EQUAL(conn_res.uris.size(), 1);
  BOOST_REQUIRE_EQUAL(conn_res.uris[0], "inproc://foo");

  auto search = req_pubsub;
  search.module_name = "*";
  conn_res = NetworkManager::get().get_preconfigured_connections(search);
  BOOST_REQUIRE_EQUAL(conn_res.uris.size(), 2);
  BOOST_REQUIRE(conn_res.uris[0] == "inproc://bar" || conn_res.uris[1] == "inproc://bar");
  BOOST_REQUIRE(conn_res.uris[0] == "inproc://rab" || conn_res.uris[1] == "inproc://rab");

  ConnectionRequest req_notfound;
  req_notfound.data_type = "blahblah";
  req_notfound.app_name = "NetworkManager_test";
  req_notfound.module_name = "Config";

  conn_res = NetworkManager::get().get_preconfigured_connections(req_notfound);
  BOOST_REQUIRE_EQUAL(conn_res.uris.size(), 0);

  BOOST_REQUIRE(!NetworkManager::get().is_pubsub_connection(req_sendrecv));
  BOOST_REQUIRE(NetworkManager::get().is_pubsub_connection(req_pubsub));
  BOOST_REQUIRE(NetworkManager::get().is_pubsub_connection(req_different_pubsub));
  BOOST_REQUIRE_EXCEPTION(NetworkManager::get().is_pubsub_connection(req_notfound),
                          ConnectionNotFound,
                          [](ConnectionNotFound const&) { return true; });

  Connections_t testConfig;
  Connection testConn;
  testConn.bind_endpoint = Endpoint{
    req_notfound.data_type, req_notfound.app_name, req_notfound.module_name, req_notfound.source_id, Direction::kInput
  };
  testConn.uri = "inproc://rab";
  testConn.connection_type = ConnectionType::kSendRecv;
  testConfig.push_back(testConn);
  BOOST_REQUIRE_EXCEPTION(
    NetworkManager::get().configure(testConfig), AlreadyConfigured, [&](AlreadyConfigured const&) { return true; });

  NetworkManager::get().reset();

  NetworkManager::get().configure(testConfig);
  conn_res = NetworkManager::get().get_preconfigured_connections(req_notfound);
  BOOST_REQUIRE_EQUAL(conn_res.uris.size(), 1);
  conn_res = NetworkManager::get().get_preconfigured_connections(req_sendrecv);
  BOOST_REQUIRE_EQUAL(conn_res.uris.size(), 0);
}

BOOST_FIXTURE_TEST_CASE(NameCollisionInConfiguration, NetworkManagerTestFixture)
{
  NetworkManager::get().reset();
  Connections_t testConfig;
  testConfig.emplace_back(Connection{ bind_endpoint_sendrecv, {}, "inproc://foo", ConnectionType::kSendRecv });
  testConfig.emplace_back(Connection{ bind_endpoint_sendrecv, {}, "inproc://bar", ConnectionType::kSendRecv });
  BOOST_REQUIRE_EXCEPTION(
    NetworkManager::get().configure(testConfig), NameCollision, [&](NameCollision const&) { return true; });
}

BOOST_AUTO_TEST_CASE(MakeIPMPlugins) {}

BOOST_AUTO_TEST_SUITE_END()

#pragma GCC diagnostic pop