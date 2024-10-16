/**
 * @file NetworkManager_test.cxx NetworkManager class Unit Tests
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/network/NetworkManager.hpp"
#include "opmonlib/TestOpMonManager.hpp"

#include "logging/Logging.hpp"

#define BOOST_TEST_MODULE NetworkManager_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <string>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

using namespace dunedaq::iomanager;

BOOST_AUTO_TEST_SUITE(NetworkManager_test)

const std::string TEST_OKS_DB = "test/config/networkmanager_test.data.xml";

struct NetworkManagerTestFixture
{
  NetworkManagerTestFixture()
  {

    confdb = std::make_shared<dunedaq::conffwk::Configuration>("oksconflibs:" + TEST_OKS_DB);
    confdb->get<dunedaq::confmodel::NetworkConnection>(connections);

    sendRecvConnId.uid = "sendRecv";
    sendRecvConnId.data_type = "data";
    pubSubConnId1.uid = "pubsub1";
    pubSubConnId1.data_type = "String";
    pubSubConnId2.uid = "pubsub2";
    pubSubConnId2.data_type = "String";
    pubSubConnId3.uid = "pubsub3";
    pubSubConnId3.data_type = "String";

    dunedaq::opmonlib::TestOpMonManager opmgr;
    NetworkManager::get().configure("NetworkManager_t", connections, nullptr, opmgr); // Not using ConfigClient
  }
  ~NetworkManagerTestFixture() { NetworkManager::get().reset(); }

  NetworkManagerTestFixture(NetworkManagerTestFixture const&) = default;
  NetworkManagerTestFixture(NetworkManagerTestFixture&&) = default;
  NetworkManagerTestFixture& operator=(NetworkManagerTestFixture const&) = default;
  NetworkManagerTestFixture& operator=(NetworkManagerTestFixture&&) = default;

  ConnectionId sendRecvConnId, pubSubConnId1, pubSubConnId2, pubSubConnId3;
  std::shared_ptr<dunedaq::conffwk::Configuration> confdb;
  std::vector<const dunedaq::confmodel::NetworkConnection*> connections;
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
  auto conn_res = NetworkManager::get().get_preconfigured_connections(sendRecvConnId);
  BOOST_REQUIRE_EQUAL(conn_res.connections.size(), 1);
  BOOST_REQUIRE_EQUAL(conn_res.connections[0].uri, "inproc://foo");

  conn_res = NetworkManager::get().get_preconfigured_connections(pubSubConnId1);
  BOOST_REQUIRE_EQUAL(conn_res.connections.size(), 1);
  BOOST_REQUIRE_EQUAL(conn_res.connections[0].uri, "inproc://bar");

  ConnectionId topicConnId;
  topicConnId.uid = ".*";
  topicConnId.data_type = "String";
  conn_res = NetworkManager::get().get_preconfigured_connections(topicConnId);
  BOOST_REQUIRE_EQUAL(conn_res.connections.size(), 3);
  BOOST_REQUIRE(conn_res.connections[0].uri == "inproc://bar" || conn_res.connections[1].uri == "inproc://bar" ||
                conn_res.connections[2].uri == "inproc://bar");
  BOOST_REQUIRE(conn_res.connections[0].uri == "inproc://rab" || conn_res.connections[1].uri == "inproc://rab" ||
                conn_res.connections[2].uri == "inproc://rab");
  BOOST_REQUIRE(conn_res.connections[0].uri == "inproc://abr" || conn_res.connections[1].uri == "inproc://abr" ||
                conn_res.connections[2].uri == "inproc://abr");

  ConnectionId id_notfound;
  id_notfound.uid = "blah";
  id_notfound.data_type = "blahblah";

  conn_res = NetworkManager::get().get_preconfigured_connections(id_notfound);
  BOOST_REQUIRE_EQUAL(conn_res.connections.size(), 0);

  id_notfound.uid = "sendRecv";
  id_notfound.data_type = "blahblah";
  conn_res = NetworkManager::get().get_preconfigured_connections(id_notfound);
  BOOST_REQUIRE_EQUAL(conn_res.connections.size(), 0);

  id_notfound.uid = "blah";
  id_notfound.data_type = "String";
  conn_res = NetworkManager::get().get_preconfigured_connections(id_notfound);
  BOOST_REQUIRE_EQUAL(conn_res.connections.size(), 0);

  BOOST_REQUIRE(!NetworkManager::get().is_pubsub_connection(sendRecvConnId));
  BOOST_REQUIRE(NetworkManager::get().is_pubsub_connection(pubSubConnId1));
  BOOST_REQUIRE(NetworkManager::get().is_pubsub_connection(topicConnId));
  BOOST_REQUIRE_EXCEPTION(NetworkManager::get().is_pubsub_connection(id_notfound),
                          ConnectionNotFound,
                          [](ConnectionNotFound const&) { return true; });

  dunedaq::opmonlib::TestOpMonManager opmgr;
  BOOST_REQUIRE_EXCEPTION(NetworkManager::get().configure("NetworkManager_t", connections, nullptr, opmgr),
                          AlreadyConfigured,
                          [&](AlreadyConfigured const&) { return true; });

  NetworkManager::get().reset();

  NetworkManager::get().configure("NetworkManager_t", connections, nullptr, opmgr);
}

BOOST_FIXTURE_TEST_CASE(GetDatatypes, NetworkManagerTestFixture)
{
  auto sendRecvDataType = NetworkManager::get().get_datatypes("sendRecv");
  BOOST_REQUIRE_EQUAL(sendRecvDataType.size(), 1);
  BOOST_REQUIRE_EQUAL(*sendRecvDataType.begin(), "data");

  auto pubsub3DataType = NetworkManager::get().get_datatypes("pubsub3");
  BOOST_REQUIRE_EQUAL(pubsub3DataType.size(), 1);
  BOOST_REQUIRE_EQUAL(*pubsub3DataType.begin(), "String");

  auto invalidDataType = NetworkManager::get().get_datatypes("NonExistentUID");
  BOOST_REQUIRE_EQUAL(invalidDataType.size(), 0);
}

BOOST_AUTO_TEST_CASE(MakeIPMPlugins) {}

BOOST_AUTO_TEST_SUITE_END()

#pragma GCC diagnostic pop
