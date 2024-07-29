/**
 * @file QueueRegistry_test.cxx QueueRegistry class Unit Tests
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/queue/QueueRegistry.hpp"

#define BOOST_TEST_MODULE QueueRegistry_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <map>
#include <string>
#include <type_traits>

BOOST_AUTO_TEST_SUITE(QueueRegistry_test)

using namespace dunedaq::iomanager;

BOOST_AUTO_TEST_CASE(Configure)
{
  std::vector<QueueConfig> queue_registry_config;
  QueueConfig qc;
  qc.id.uid = "test_queue_unknown";
  qc.queue_type = QueueType::kUnknown;
  qc.capacity = 10;
  queue_registry_config.push_back(qc);
  qc.queue_type = QueueType::kStdDeQueue;
  qc.capacity = 10;
  qc.id.uid = "test_queue_stddeque";
  queue_registry_config.push_back(qc);
  qc.queue_type = QueueType::kFollySPSCQueue;
  qc.capacity = 10;
  qc.id.uid = "test_queue_fspsc";
  queue_registry_config.push_back(qc);
  qc.queue_type = QueueType::kFollyMPMCQueue;
  qc.capacity = 10;
  qc.id.uid = "test_queue_fmpmc";
  queue_registry_config.push_back(qc);

  QueueRegistry::get().configure(queue_registry_config);

  BOOST_REQUIRE_EXCEPTION(QueueRegistry::get().configure(queue_registry_config),
                          QueueRegistryConfigured,
                          [&](QueueRegistryConfigured const&) { return true; });
}


BOOST_AUTO_TEST_CASE(CreateQueue)
{

  auto queue_ptr_deque = QueueRegistry::get().get_queue<int>("test_queue_stddeque");
  BOOST_REQUIRE(queue_ptr_deque != nullptr);
  auto queue_ptr_fspsc = QueueRegistry::get().get_queue<int>("test_queue_fspsc");
  BOOST_REQUIRE(queue_ptr_fspsc != nullptr);
  auto queue_ptr_fmpmc = QueueRegistry::get().get_queue<int>("test_queue_fmpmc");
  BOOST_REQUIRE(queue_ptr_fmpmc != nullptr);
  BOOST_REQUIRE_EXCEPTION(QueueRegistry::get().get_queue<int>("test_queue_unknown"),
                          QueueTypeUnknown,
                          [&](QueueTypeUnknown const&) { return true; });
}

BOOST_AUTO_TEST_SUITE_END()
