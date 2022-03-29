/**
 * @file FakeTRB_test.cxx Demonstration of simplified TRB using iomanager
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2021.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/IOManager.hpp"

#include "appfwk/QueueRegistry.hpp"
#include "appfwk/app/Nljs.hpp"
#include "daqdataformats/TriggerRecord.hpp"
#include "dfmessages/DataRequest.hpp"
#include "dfmessages/Fragment_serialization.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "networkmanager/NetworkManager.hpp"
#include "networkmanager/nwmgr/Structs.hpp"

#include "serialization/Serialization.hpp"

#define BOOST_TEST_MODULE FakeTRB_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <string>
#include <vector>

using namespace dunedaq::iomanager;

BOOST_AUTO_TEST_SUITE(FakeTRB_test)

struct ConfigurationTestFixture
{
  ConfigurationTestFixture()
  {
    dunedaq::networkmanager::nwmgr::Connections nwCfg;
    dunedaq::networkmanager::nwmgr::Connection testConn;
    testConn.name = "td_connection";
    testConn.address = "inproc://td";
    nwCfg.push_back(testConn);
    testConn.name = "dr_connection";
    testConn.address = "inproc://cr";
    nwCfg.push_back(testConn);
    testConn.name = "frag_connection";
    testConn.address = "inproc://frag";
    nwCfg.push_back(testConn);
    dunedaq::networkmanager::NetworkManager::get().configure(nwCfg);

    std::map<std::string, dunedaq::appfwk::QueueConfig> config_map;
    dunedaq::appfwk::QueueConfig qspec;
    qspec.kind = dunedaq::appfwk::QueueConfig::queue_kind::kStdDeQueue;
    qspec.capacity = 10;
    config_map["trigger_record_queue"] = qspec;
    dunedaq::appfwk::QueueRegistry::get().configure(config_map);
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
};

namespace {

struct FakeTRBTracking
{
  std::map<dunedaq::dfmessages::trigger_number_t, std::unique_ptr<dunedaq::daqdataformats::TriggerRecord>> tr_map;
  std::mutex tr_map_mutex;
  std::vector<dunedaq::daqdataformats::GeoID> components;
  std::atomic<size_t> trs_processed{ 0 };
  std::atomic<bool> test_running{ false };
  std::atomic<size_t> tokens{ 10 };
};
static FakeTRBTracking test_tracking;

void
mlt_thread()
{
  TLOG() << "Starting MLT thread";
  dunedaq::dfmessages::timestamp_t next_timestamp = 1;
  dunedaq::dfmessages::trigger_number_t next_trigger_number = 1;
  dunedaq::iomanager::IOManager iom;
  dunedaq::iomanager::ConnectionID td_conn{ "network", "td_connection", "" };
  auto sender = iom.get_sender<dunedaq::dfmessages::TriggerDecision>(td_conn);

  while (test_tracking.test_running) {
    dunedaq::dfmessages::TriggerDecision td;
    td.trigger_number = next_trigger_number;
    ++next_trigger_number;
    td.trigger_timestamp = next_timestamp;
    next_timestamp += 1000;
    std::vector<dunedaq::daqdataformats::ComponentRequest> components;

    for (auto& comp : test_tracking.components) {
      dunedaq::daqdataformats::ComponentRequest request;
      request.component = comp;
      request.window_begin = td.trigger_timestamp;
      request.window_end = next_timestamp;
      components.push_back(request);
    }

    td.components = components;

    TLOG() << "Sending TriggerDecision " << td.trigger_number << " with " << td.components.size() << " components";
    sender->send(td, dunedaq::iomanager::Sender::s_block);

    --test_tracking.tokens;
    while (test_tracking.tokens.load() < 1) {
      usleep(10000);
    }
  }

  TLOG() << "Ending MLT thread";
}

// In TriggerRecordBuilder
void
trigger_decision_callback(dunedaq::dfmessages::TriggerDecision& td)
{
  auto trigger_number = td.trigger_number;
  TLOG() << "Received TriggerDecision with trigger number " << trigger_number;
  {
    std::lock_guard<std::mutex> lk(test_tracking.tr_map_mutex);
    test_tracking.tr_map[trigger_number] = std::make_unique<dunedaq::daqdataformats::TriggerRecord>(td.components);
  }

  dunedaq::iomanager::IOManager iom;
  dunedaq::iomanager::ConnectionID dr_conn{ "network", "dr_connection", "" };
  auto sender = iom.get_sender<dunedaq::dfmessages::DataRequest>(dr_conn);

  for (auto& comp : td.components) {
    dunedaq::dfmessages::DataRequest req;
    req.trigger_number = td.trigger_number;
    req.request_information = comp;
    req.data_destination = "frag_connection";
    TLOG() << "Sending DataRequest for trigger_number " << req.trigger_number << " to element "
           << comp.component.element_id;
    sender->send(req, dunedaq::iomanager::Sender::s_block);
  }
  TLOG() << "End TriggerDecision callback";
}

// In Component
void
data_request_callback(dunedaq::dfmessages::DataRequest& dr)
{
  TLOG() << "Received DataRequest with trigger_number " << dr.trigger_number;
  dunedaq::daqdataformats::FragmentHeader header;
  header.trigger_number = dr.trigger_number;
  header.trigger_timestamp = dr.trigger_timestamp;
  header.run_number = dr.run_number;
  header.element_id = dr.request_information.component;

  auto buf1 = malloc(10);
  auto frag = std::make_unique<dunedaq::daqdataformats::Fragment>(buf1, size_t(10));
  frag->set_header_fields(header);
  free(buf1);

  TLOG() << "Sending Fragment with trigger_number " << dr.trigger_number << " and element "
         << dr.request_information.component.element_id;
  dunedaq::iomanager::IOManager iom;
  dunedaq::iomanager::ConnectionID frag_conn{ "network", "frag_connection", "" };
  auto sender = iom.get_sender<std::unique_ptr<dunedaq::daqdataformats::Fragment>>(frag_conn);
  sender->send(frag, dunedaq::iomanager::Sender::s_block);
  TLOG() << "End DataRequest callback";
}

// In TriggerRecordBuilder
void
fragment_callback(std::unique_ptr<dunedaq::daqdataformats::Fragment>& frag)
{
  if (frag == nullptr)
    return;
  auto trigger_number = frag->get_trigger_number();
  TLOG() << "Received Fragment with trigger_number " << trigger_number << " and element_id "
         << frag->get_element_id().element_id;
  std::lock_guard<std::mutex> lk(test_tracking.tr_map_mutex);

  if (!test_tracking.tr_map.count(trigger_number))
    return;

  test_tracking.tr_map.at(trigger_number)->add_fragment(std::move(frag));

  if (test_tracking.tr_map.at(trigger_number)->get_fragments_ref().size() == test_tracking.components.size()) {
    dunedaq::iomanager::IOManager iom;
    dunedaq::iomanager::ConnectionID tr_conn{ "queue", "trigger_record_queue", "" };
    auto sender = iom.get_sender<std::unique_ptr<dunedaq::daqdataformats::TriggerRecord>>(tr_conn);
    sender->send(test_tracking.tr_map[trigger_number], dunedaq::iomanager::Sender::s_block);
    test_tracking.tr_map.erase(trigger_number);
  }
  TLOG() << "End Fragment callback";
}

// In DataWriter
void
trigger_record_callback(std::unique_ptr<dunedaq::daqdataformats::TriggerRecord>& tr)
{
  BOOST_REQUIRE_EQUAL(tr->get_fragments_ref().size(), tr->get_header_ref().get_num_requested_components());
  test_tracking.trs_processed++;
  test_tracking.tokens++;
  TLOG() << "End TriggerRecord callback";
}
} // namespace ""

BOOST_FIXTURE_TEST_CASE(SimpleTRBFlow, ConfigurationTestFixture)
{
  TLOG() << "SimpleTRBFlow Test BEGIN";

  TLOG() << "Adding components";
  dunedaq::daqdataformats::GeoID g;
  g.element_id = 1;
  test_tracking.components.push_back(g);
  g.element_id = 2;
  test_tracking.components.push_back(g);

  IOManager iom;
  dunedaq::iomanager::ConnectionID td_conn{ "network", "td_connection", "" };
  dunedaq::iomanager::ConnectionID dr_conn{ "network", "dr_connection", "" };
  dunedaq::iomanager::ConnectionID frag_conn{ "network", "frag_connection", "" };
  dunedaq::iomanager::ConnectionID tr_conn{ "queue", "trigger_record_queue", "" };

  TLOG() << "Setting callbacks";
  iom.add_callback<dunedaq::dfmessages::TriggerDecision>(td_conn, trigger_decision_callback);
  iom.add_callback<dunedaq::dfmessages::DataRequest>(dr_conn, data_request_callback);
  iom.add_callback<std::unique_ptr<dunedaq::daqdataformats::Fragment>>(frag_conn, fragment_callback);
  iom.add_callback<std::unique_ptr<dunedaq::daqdataformats::TriggerRecord>>(tr_conn,
                                                                            trigger_record_callback);

  TLOG() << "Starting test";
  test_tracking.test_running = true;
  std::thread t(&mlt_thread);

  TLOG() << "Waiting for TriggerRecords to be processed";
  while (test_tracking.trs_processed < 100) {
    usleep(1000);
  }

  TLOG() << "Ending test";
  test_tracking.test_running = false;
  t.join();
  BOOST_REQUIRE_GE(test_tracking.trs_processed.load(), 100);

  iom.remove_callback<dunedaq::dfmessages::TriggerDecision>(td_conn);
  iom.remove_callback<dunedaq::dfmessages::DataRequest>(dr_conn);
  iom.remove_callback<std::unique_ptr<dunedaq::daqdataformats::Fragment>>(frag_conn);
  iom.remove_callback<std::unique_ptr<dunedaq::daqdataformats::TriggerRecord>>(tr_conn);

  TLOG() << "SimpleTRBFlow Test END";
}

BOOST_AUTO_TEST_SUITE_END()
