/**
 * @file reconnection_test.cxx
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/IOManager.hpp"
#include "logging/Logging.hpp"

#include "boost/program_options.hpp"

#include <algorithm>
#include <execution>
#include <fstream>
#include <sys/prctl.h>

namespace dunedaq {
namespace iomanager {
struct Data
{
  size_t seq_number;
  size_t sender_id;
  std::vector<uint8_t> contents;

  Data() = default;
  Data(size_t seq, size_t id, size_t size)
    : seq_number(seq)
    , sender_id(id)
    , contents(size)
  {
  }
  virtual ~Data() = default;
  Data(Data const&) = default;
  Data& operator=(Data const&) = default;
  Data(Data&&) = default;
  Data& operator=(Data&&) = default;

  DUNE_DAQ_SERIALIZE(Data, seq_number, sender_id, contents);
};

struct TestConfig
{
  bool use_connectivity_service = false;
  int port = 5000;
  std::string server = "localhost";
  size_t num_apps = 10;
  size_t my_offset = 0;
  size_t message_size_kb = 10;
  size_t message_interval_ms = 1000;
  int publish_interval = 1000;
  bool verbose = false;
  static std::atomic<bool> test_running;

  void configure_connsvc()
  {
    setenv("CONNECTION_SERVER", server.c_str(), 1);
    setenv("CONNECTION_PORT", std::to_string(port).c_str(), 1);
  }

  static std::string get_connection_uri(size_t conn_id)
  {
    size_t conn_ip = conn_id + 1;
    int first_byte = (conn_ip << 1) & 0xFF;
    if (first_byte == 0)
      first_byte = 1;
    int second_byte = (conn_ip >> 7) & 0xFF;
    int third_byte = (conn_ip >> 15) & 0xFF;
    std::string conn_addr = "tcp://127." + std::to_string(third_byte) + "." + std::to_string(second_byte) + "." +
                            std::to_string(first_byte) + ":15500";

    return conn_addr;
  }

  static std::string get_connection_name(size_t conn_id)
  {
    std::stringstream ss;
    ss << "conn_" << std::hex << std::internal << std::showbase << std::setw(10) << std::setfill('0')
       << conn_id; // The tempatation to use printf is strong...
    return ss.str();
  }

  size_t get_send_id() { return (my_offset + 1) % num_apps; }

  void configure_iomanager()
  {
    setenv("DUNEDAQ_PARTITION", "iomanager_stress_test", 0);

    Queues_t queues;
    Connections_t connections;

    auto recv_conn = Connection{ ConnectionId{ get_connection_name(my_offset), "data_t" },
                                 get_connection_uri(my_offset),
                                 ConnectionType::kSendRecv };
    connections.push_back(recv_conn);

    if (!use_connectivity_service) {
      auto send_conn = Connection{ ConnectionId{ get_connection_name(get_send_id()), "data_t" },
                                   get_connection_uri(get_send_id()),
                                   ConnectionType::kSendRecv };
      connections.push_back(send_conn);
    }

    IOManager::get()->configure(queues, connections, use_connectivity_service /*, publish_interval*/);
  }
};

}
// Must be in dunedaq namespace only
DUNE_DAQ_SERIALIZABLE(iomanager::Data, "data_t");
}

static void
signal_handler(int signum)
{
  TLOG() << "Received signal of type " << signum << ", setting test_running to false!";
  dunedaq::iomanager::TestConfig::test_running = false;
}

std::atomic<bool> dunedaq::iomanager::TestConfig::test_running{ true };

int
main(int argc, char* argv[])
{
  dunedaq::logging::Logging::setup();
  dunedaq::iomanager::TestConfig config;
  size_t id = 0;

  namespace po = boost::program_options;
  po::options_description desc("Program to test IOManager load with many connections");
  desc.add_options()("use_connectivity_service,c",
                     po::bool_switch(&config.use_connectivity_service),
                     "enable the ConnectivityService in IOManager")(
    "num_apps,n", po::value<size_t>(&config.num_apps), "Number of applications in the ring")(
    "port,p", po::value<int>(&config.port), "port to connect to on configuration server")(
    "server,s", po::value<std::string>(&config.server), "Configuration server to connect to")(
    "message_size_kb,z", po::value<size_t>(&config.message_size_kb), "Size of each message, in KB")(
    "message_interval_ms,r", po::value<size_t>(&config.message_interval_ms), "Time to wait between messages, in ms")(
    "id", po::value<size_t>(&id), "Specific app ID to start (note that ID 0 will start num_apps)")(
    "publish_interval,i",
    po::value<int>(&config.publish_interval),
    "Interval, in ms, for ConfigClient to re-publish connection info")(
    "verbose,v", po::bool_switch(&config.verbose), "print more verbose output");

  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (std::exception& ex) {
    std::cerr << "Error parsing command line " << ex.what() << std::endl;
    std::cerr << desc << std::endl;
    return 0;
  }

  if (config.use_connectivity_service) {
    TLOG() << "Setting Connectivity Service Environment variables";
    config.configure_connsvc();
  }

  config.my_offset = id;
  if (config.my_offset == 0) {
    for (size_t ii = 1; ii < config.num_apps; ++ii) {
      auto new_id = fork();
      if (new_id == 0) {
        config.my_offset = ii;
        break;
      }
    }
  }

  std::string app_name = "recon_test_" + std::to_string(config.my_offset);
  int s;
  s = prctl(PR_SET_NAME, app_name.c_str(), NULL, NULL, NULL);
  TLOG() << "Set app name to " << app_name << " had status " << s;

  TLOG() << "Configuring IOManager";
  config.configure_iomanager();

  struct sigaction action;
  action.sa_handler = signal_handler;
  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGINT);
  action.sa_flags = 0;
  sigaction(SIGINT, &action, nullptr);

  size_t msg_idx = 0;

  while (dunedaq::iomanager::TestConfig::test_running.load()) {
    if (!(config.my_offset == 0 && msg_idx == 0)) {
      auto msg = dunedaq::iomanager::IOManager::get()
                   ->get_receiver<dunedaq::iomanager::Data>(config.get_connection_name(config.my_offset))
                   ->try_receive(std::chrono::milliseconds(2 * config.message_interval_ms));
      if(msg) {
        msg_idx = msg->seq_number;
        TLOG() << "Received message " << msg_idx << " from " << msg->sender_id << " with size " << msg->contents.size();
      } else {
        TLOG() << "DID NOT RECEIVE message with expected index " << (msg_idx + 1) << "! Continuing with send...";
        if(config.my_offset != 0) {msg_idx++;}
      }
    }

    if (config.my_offset == 0) {
      msg_idx++;
    }

    bool send_success = false;
    while (!send_success) {
      dunedaq::iomanager::Data msg(msg_idx, config.my_offset, config.message_size_kb * 1024);
      send_success = dunedaq::iomanager::IOManager::get()
                       ->get_sender<dunedaq::iomanager::Data>(config.get_connection_name(config.get_send_id()))
                       ->try_send(std::move(msg), dunedaq::iomanager::Sender::s_block);
      if(!send_success) TLOG() << "try_send call failed, retrying...";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(config.message_interval_ms));
  }

  TLOG() << "Cleaning up";
  dunedaq::iomanager::IOManager::get()->reset();
  TLOG() << "DONE";
};