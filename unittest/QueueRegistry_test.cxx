/**
 * @file QueueRegistry_test.cxx QueueRegistry class Unit Tests
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/queue/QueueRegistry.hpp"
#include "opmonlib/TestOpMonManager.hpp"

#include "conffwk/Configuration.hpp"

#define BOOST_TEST_MODULE QueueRegistry_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <map>
#include <string>
#include <type_traits>

BOOST_AUTO_TEST_SUITE(QueueRegistry_test)

using namespace dunedaq::iomanager;

const std::string TEST_OKS_DB = "test/config/queueregistry_test.data.xml";

struct ConfigurationFixture
{
  ConfigurationFixture()
  {
    confdb = std::make_shared<dunedaq::conffwk::Configuration>("oksconflibs:" + TEST_OKS_DB);
    confdb->get<dunedaq::confmodel::Queue>(queues);

  };
  static std::shared_ptr<dunedaq::conffwk::Configuration> confdb;
  static std::vector<const dunedaq::confmodel::Queue*> queues;
};
std::vector<const dunedaq::confmodel::Queue*> ConfigurationFixture::queues;
std::shared_ptr<dunedaq::conffwk::Configuration> ConfigurationFixture::confdb(nullptr);
BOOST_TEST_GLOBAL_FIXTURE(ConfigurationFixture);

BOOST_AUTO_TEST_CASE(Configure)
{
  dunedaq::opmonlib::TestOpMonManager opmgr;
  QueueRegistry::get().configure(ConfigurationFixture::queues, opmgr);

  BOOST_REQUIRE_EXCEPTION(QueueRegistry::get().configure(ConfigurationFixture::queues, opmgr),
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
