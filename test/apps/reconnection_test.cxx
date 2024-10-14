/**
 * @file reconnection_test.cxx
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/IOManager.hpp"
#include "logging/Logging.hpp"
#include "opmonlib/TestOpMonManager.hpp"

#include "boost/program_options.hpp"

#include <algorithm>
#include <execution>
#include <fstream>
#include <random>
#include <sys/prctl.h>
#include <sys/wait.h>

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
  int port = 5000;
  std::string server = "localhost";
  size_t num_apps = 10;
  size_t my_offset = 0;
  size_t message_size_kb = 10;
  size_t message_interval_ms = 500;
  int publish_interval = 1000;
  bool verbose = false;
  static std::atomic<bool> test_running;

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
    Queues_t queues;
    Connections_t connections;

    auto recv_conn = Connection{ ConnectionId{ get_connection_name(my_offset), "data_t" },
                                 "tcp://" + std::string(host) + ":*",
                                 ConnectionType::kSendRecv };
    connections.push_back(recv_conn);

    dunedaq::opmonlib::TestOpMonManager opmgr;
    IOManager::get()->configure(queues, connections, true, std::chrono::milliseconds(publish_interval), opmgr);
  }

  void send_message(uint8_t msg_idx)
  {
    TLOG_DEBUG(5) << "RCTApp" << my_offset << ": "
                  << "Sending message " << static_cast<int>(msg_idx) << " to " << get_send_id();
    Data msg(msg_idx, my_offset, message_size_kb * 1024);
    IOManager::get()
      ->get_sender<Data>(get_connection_name(get_send_id()))
      ->try_send(std::move(msg), std::chrono::milliseconds(message_interval_ms));
  }
};

}
// Must be in dunedaq namespace only
DUNE_DAQ_SERIALIZABLE(iomanager::Data, "data_t");
}

static void
signal_handler(int signum)
{
  TLOG_DEBUG(3) << "Received signal of type " << signum << ", setting test_running to false!";
  dunedaq::iomanager::TestConfig::test_running = false;
}

std::atomic<bool> dunedaq::iomanager::TestConfig::test_running{ true };

void
run_test(dunedaq::iomanager::TestConfig config, uint8_t msg_idx = 0)
{

  std::string app_name = "recon_test_" + std::to_string(config.my_offset);
  int s;
  s = prctl(PR_SET_NAME, app_name.c_str(), NULL, NULL, NULL);
  TLOG_DEBUG(3) << "RCTApp" << config.my_offset << ": "
                << "Set app name to " << app_name << " had status " << s;

  TLOG_DEBUG(3) << "RCTApp" << config.my_offset << ": "
                << "Configuring IOManager";
  config.configure_iomanager();

  struct sigaction action;
  action.sa_handler = signal_handler;
  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGINT);
  action.sa_flags = 0;
  sigaction(SIGINT, &action, nullptr);

  // Send first message
  if (config.my_offset == 0) {
    config.send_message(msg_idx);
  }

  try {
    while (dunedaq::iomanager::TestConfig::test_running.load()) {

      auto msg = dunedaq::iomanager::IOManager::get()
                   ->get_receiver<dunedaq::iomanager::Data>(config.get_connection_name(config.my_offset))
                   ->try_receive(std::chrono::milliseconds(3 * config.message_interval_ms));
      if (msg) {
        msg_idx = msg->seq_number;
        TLOG_DEBUG(5) << "RCTApp" << config.my_offset << ": "
                      << "Received message " << static_cast<int>(msg_idx) << " from " << msg->sender_id << " with size "
                      << msg->contents.size();

        if (config.my_offset == 0) {
          TLOG() << "Message " << static_cast<int>(msg_idx) << " successfully passed through the ring";
          ++msg_idx;
          std::this_thread::sleep_for(std::chrono::milliseconds(config.message_interval_ms));
        }

        config.send_message(msg_idx);

      } else {
        TLOG_DEBUG(4) << "RCTApp" << config.my_offset << ": "
                      << "DID NOT RECEIVE message with expected index " << static_cast<int>(msg_idx + 1);

        if (config.my_offset == 0) {
          ++msg_idx;
          std::this_thread::sleep_for(std::chrono::milliseconds(config.message_interval_ms));
          config.send_message(msg_idx);
        }
      }
    }
  } catch (ers::Issue const& e) {
    TLOG_DEBUG(2) << "RCTApp" << config.my_offset << ": Received exception " << e;
  }

  TLOG_DEBUG(3) << "RCTApp" << config.my_offset << ": "
                << "Cleaning up";
  dunedaq::iomanager::IOManager::get()->reset();
  TLOG_DEBUG(3) << "RCTApp" << config.my_offset << ": "
                << "DONE";

  exit(msg_idx);
}

int
main(int argc, char* argv[])
{
  dunedaq::logging::Logging::setup();
  dunedaq::iomanager::TestConfig config;

  size_t duration = 60;
  size_t kill_interval = 5000;
  bool help_requested = false;

  namespace po = boost::program_options;
  po::options_description desc("Program to test IOManager load with many connections");
  desc.add_options()("num_apps,n",
                     po::value<size_t>(&config.num_apps)->default_value(config.num_apps),
                     "Number of applications in the ring")(
    "port,p", po::value<int>(&config.port)->default_value(config.port), "port to connect to on configuration server")(
    "server,s",
    po::value<std::string>(&config.server)->default_value(config.server),
    "Configuration server to connect to")(
    "message_size_kb,z",
    po::value<size_t>(&config.message_size_kb)->default_value(config.message_size_kb),
    "Size of each message, in KB")(
    "message_interval_ms,r",
    po::value<size_t>(&config.message_interval_ms)->default_value(config.message_interval_ms),
    "Time to wait between messages, in ms")(
    "test_duration_s,D", po::value<size_t>(&duration)->default_value(duration), "Length of the test, in seconds")(
    "kill_interval_ms,k",
    po::value<size_t>(&kill_interval)->default_value(kill_interval),
    "Randomly kill one app in ring each interval, in ms")(
    "publish_interval,i",
    po::value<int>(&config.publish_interval)->default_value(config.publish_interval),
    "Interval, in ms, for ConfigClient to re-publish connection info")(
    "verbose,v", po::bool_switch(&config.verbose)->default_value(config.verbose), "print more verbose output")(
    "help,h", po::bool_switch(&help_requested)->default_value(help_requested), "Print this help message");

  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (std::exception& ex) {
    std::cerr << "Error parsing command line " << ex.what() << std::endl;
    std::cerr << desc << std::endl;
    return 1;
  }

  if (help_requested) {
    std::cout << desc << std::endl;
    return 0;
  }

  TLOG() << "Setting Connectivity Service Environment variables";
  config.configure_connsvc();

  TLOG() << "Starting test";
  std::vector<pid_t> pids;
  for (size_t ii = 0; ii < config.num_apps; ++ii) {
    auto new_id = fork();
    if (new_id == 0) {
      config.my_offset = ii;
      run_test(config);
    } else {
      pids.push_back(new_id);
    }
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(0, config.num_apps - 1);

  TLOG() << "Waiting " << kill_interval << " ms for things to get started";
  std::this_thread::sleep_for(std::chrono::milliseconds(kill_interval));

  auto test_start = std::chrono::steady_clock::now();
  while (std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(std::chrono::steady_clock::now() -
                                                                                  test_start)
           .count() < duration) {
    auto app_to_kill = distrib(gen);
    TLOG() << "Sending SIGINT to app " << app_to_kill << ", at PID " << pids[app_to_kill];
    kill(pids[app_to_kill], SIGINT);
    siginfo_t status;
    auto sts = waitid(P_PID, pids[app_to_kill], &status, WEXITED);
    TLOG_DEBUG(6) << "Forked process " << pids[app_to_kill] << " exited with status " << status.si_status
                  << " (wait status " << sts << ")";

    TLOG() << "App exited, waiting for 2 message intervals";
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * config.message_interval_ms));

    TLOG() << "Starting replacement app";
    auto new_id = fork();
    if (new_id == 0) {
      config.my_offset = app_to_kill;
      run_test(config, sts);
    } else {
      pids[app_to_kill] = new_id;
    }

    TLOG() << "Waiting for another " << kill_interval << " ms";
    std::this_thread::sleep_for(std::chrono::milliseconds(kill_interval));
  }

  TLOG() << "Reaping children";
  for (size_t ii = 0; ii < config.num_apps; ++ii) {
    TLOG() << "Sending SIGINT to app " << ii << ", at PID " << pids[ii];
    kill(pids[ii], SIGINT);
  }
  for (size_t ii = 0; ii < config.num_apps; ++ii) {
    TLOG() << "Sending SIGINT to app " << ii << ", at PID " << pids[ii];
    kill(pids[ii], SIGKILL);
  }
  for (size_t ii = 0; ii < config.num_apps; ++ii) {
    TLOG() << "Waiting for app " << ii << " at pid " << pids[ii];
    siginfo_t status;
    auto sts = waitid(P_PID, pids[ii], &status, WEXITED);

    TLOG() << "Forked process " << pids[ii] << " exited with status " << status.si_status << " (wait status " << sts
           << ")";
  }
  TLOG() << "DONE";
}
