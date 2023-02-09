/**
 * @file iomanager_stress_test.cxx
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
#include <sys/wait.h>

namespace dunedaq {
namespace iomanager {
struct Data
{
  size_t seq_number;
  std::vector<uint8_t> contents;

  Data() = default;
  Data(size_t seq, size_t size)
    : seq_number(seq)
    , contents(size)
  {
  }
  virtual ~Data() = default;
  Data(Data const&) = default;
  Data& operator=(Data const&) = default;
  Data(Data&&) = default;
  Data& operator=(Data&&) = default;

  DUNE_DAQ_SERIALIZE(Data, seq_number, contents);
};

struct Control
{
  size_t receiver_id;
  size_t run_number;
  bool ready_to_receive;
  bool done_receiving;

  Control() = default;
  Control(size_t id, size_t run, bool ready, bool done)
    : receiver_id(id)
    , run_number(run)
    , ready_to_receive(ready)
    , done_receiving(done)
  {
  }

  DUNE_DAQ_SERIALIZE(Control, receiver_id, run_number, ready_to_receive, done_receiving);
};

struct TestConfig
{

  bool use_connectivity_service = false;
  int port = 5000;
  std::string server = "localhost";
  std::string info_file_base = "iom_stress_test";
  size_t num_apps = 1;
  size_t num_connections = 1;
  size_t num_messages = 1;
  size_t message_size_kb = 1024;
  size_t num_runs = 1;
  size_t my_id = 0;
  int publish_interval = 1000;

  void configure_connsvc()
  {
    setenv("CONNECTION_SERVER", server.c_str(), 1);
    setenv("CONNECTION_PORT", std::to_string(port).c_str(), 1);
  }

  std::string get_connection_name(size_t conn_id)
  {
    std::stringstream ss;
    ss << "conn_" << std::hex << std::internal << std::showbase << std::setw(10) << std::setfill('0')
       << conn_id + my_id * num_connections; // The tempatation to use printf is strong...
    return ss.str();
  }

  void configure_iomanager(bool is_receiver)
  {
    setenv("DUNEDAQ_PARTITION", "iomanager_stress_test", 0);
    bool configure_connections = is_receiver || !use_connectivity_service;

    Queues_t queues;
    Connections_t connections;

    if (configure_connections) {
      assert(num_apps * num_connections < (1 << 23));
      for (size_t ii = 0; ii < num_connections; ++ii) {
        size_t conn_ip = ii + 1 + (my_id * num_connections);
        int first_byte = (conn_ip << 1) & 0xFF;
        if (first_byte == 0)
          first_byte = 1;
        int second_byte = (conn_ip >> 7) & 0xFF;
        int third_byte = (conn_ip >> 15) & 0xFF;
        std::string conn_addr = "tcp://127." + std::to_string(third_byte) + "." + std::to_string(second_byte) + "." +
                                std::to_string(first_byte) + ":15500";

        TLOG_DEBUG(1) << "Adding connection with id " << get_connection_name(ii) << " and address " << conn_addr;

        connections.emplace_back(
          Connection{ ConnectionId{ get_connection_name(ii), "data_t" }, conn_addr, ConnectionType::kSendRecv });
      }

      auto port = 13000 + my_id;
      std::string conn_addr = "tcp://127.0.0.1:" + std::to_string(port);
      TLOG_DEBUG(1) << "Adding control connection conn_" << my_id << "_control with address " << conn_addr;

      connections.emplace_back(Connection{ ConnectionId{ "conn_" + std::to_string(my_id) + "_control", "control_t" },
                                           conn_addr,
                                           ConnectionType::kPubSub });
    }
    IOManager::get()->configure(
      queues, connections, use_connectivity_service, std::chrono::milliseconds(publish_interval));
  }
};
struct ReceiverTest
{
  struct ReceiverInfo
  {
    size_t last_sequence_received{ 0 };
    std::atomic<size_t> msgs_received{ 0 };
    std::atomic<size_t> msgs_with_error{ 0 };
    std::chrono::milliseconds get_receiver_time;
    std::chrono::milliseconds add_callback_time;
    std::atomic<bool> first_received{ false };
  };
  std::unordered_map<size_t, std::shared_ptr<ReceiverInfo>> receivers;
  TestConfig config;

  explicit ReceiverTest(TestConfig c)
    : config(c)
  {
  }

  void receive(size_t run_number)
  {
    auto start = std::chrono::steady_clock::now();
    auto control_sender = dunedaq::get_iom_sender<Control>("conn_" + std::to_string(config.my_id) + "_control");
    auto after_control = std::chrono::steady_clock::now();

    for (size_t conn_id = 0; conn_id < config.num_connections; ++conn_id) {
      auto info = std::make_shared<ReceiverInfo>();
      receivers[conn_id] = info;
    }

    auto after_info = std::chrono::steady_clock::now();
    std::for_each(std::execution::par_unseq,
                  std::begin(receivers),
                  std::end(receivers),
                  [&](std::pair<size_t, std::shared_ptr<ReceiverInfo>> info_pair) {
                    auto conn_id = info_pair.first;
                    auto info = info_pair.second;
                    auto recv_proc = [=](Data& msg) {
                      TLOG_DEBUG(3) << "Received message " << msg.seq_number << " with size " << msg.contents.size()
                                    << " bytes from connection " << config.get_connection_name(conn_id);

                      if (msg.contents.size() != config.message_size_kb * 1024 ||
                          msg.seq_number != info->last_sequence_received + 1) {
                        info->msgs_with_error++;
                      }
                      info->first_received = true;
                      info->last_sequence_received = msg.seq_number;
                      info->msgs_received++;
                    };

                    auto before_receiver = std::chrono::steady_clock::now();
                    auto receiver = dunedaq::get_iom_receiver<Data>(config.get_connection_name(conn_id));
                    auto after_receiver = std::chrono::steady_clock::now();
                    receiver->add_callback(recv_proc);
                    auto after_callback = std::chrono::steady_clock::now();
                    info->get_receiver_time =
                      std::chrono::duration_cast<std::chrono::milliseconds>(after_receiver - before_receiver);
                    info->add_callback_time =
                      std::chrono::duration_cast<std::chrono::milliseconds>(after_callback - after_receiver);
                  });
    auto after_callbacks = std::chrono::steady_clock::now();

    while (!std::any_of(receivers.begin(), receivers.end(), [](std::pair<size_t, std::shared_ptr<ReceiverInfo>> info) {
      return info.second->first_received.load();
    })) {
      Control msg(config.my_id, run_number, true, false);
      control_sender->send(std::move(msg), Sender::s_block);
      std::this_thread::sleep_for(10ms);
    }

    bool all_done = false;
    while (!all_done) {
      size_t recvrs_done = 0;
      for (auto& receiver : receivers) {
        if (receiver.second->msgs_received.load() == config.num_messages)
          recvrs_done++;
      }
      all_done = recvrs_done == config.num_connections;
    }
    auto after_receives = std::chrono::steady_clock::now();

    for (auto& info : receivers) {
      auto receiver = dunedaq::get_iom_receiver<Data>(config.get_connection_name(info.first));
      receiver->remove_callback();
    }
    auto after_cleanup = std::chrono::steady_clock::now();

    TLOG_DEBUG(2) << "Sending Control message indicating completion from ID " << config.my_id << " for run "
                  << run_number;

    Control msg2(config.my_id, run_number, false, true);

    control_sender->send(std::move(msg2), Sender::s_block);

    auto after_control_send = std::chrono::steady_clock::now();

    int min_get_time = std::numeric_limits<int>::max();
    int max_get_time = 0;
    int sum_get_time = 0;
    int min_cb_time = std::numeric_limits<int>::max();
    int max_cb_time = 0;
    int sum_cb_time = 0;
    for (auto& info : receivers) {
      auto get_time = info.second->get_receiver_time.count();
      auto cb_time = info.second->add_callback_time.count();
      if (get_time > max_get_time)
        max_get_time = get_time;
      if (get_time < min_get_time)
        min_get_time = get_time;
      sum_get_time += get_time;
      if (cb_time > max_cb_time)
        max_cb_time = cb_time;
      if (cb_time < min_cb_time)
        min_cb_time = cb_time;
      sum_cb_time += cb_time;
    }
    double average_get_time = static_cast<double>(sum_get_time) / static_cast<double>(config.num_connections);
    double average_cb_time = static_cast<double>(sum_cb_time) / static_cast<double>(config.num_connections);

    std::string info_file_name = config.info_file_base + "_receiver.csv";

    struct stat buffer;
    bool file_exists = stat(info_file_name.c_str(), &buffer) == 0;

    std::ofstream of(info_file_name, std::ios::app);

    if (!file_exists) {
      of << "APP_TYPE,APP_ID,RUN,N_APPS,N_CONN,N_MESS,MESS_SZ_KB,CONTROL_MS,COUNTERS_MS,ADD_CALLBACKS_MS,"
            "RECV_COMPLETE_MS,RM_CALLBACKS_MS,SEND_COMPMSG_MS,RECV_MS,MIN_GET_RECVR_TIME,MAX_GET_RECVR_TIME,AVG_GET_"
            "RECVR_TIME,MIN_ADD_CB_TIME,MAX_ADD_CB_TIME,AVG_ADD_CB_TIME,TOTAL_MS"
         << std::endl;
    }

    of << "R"
       << "," << config.my_id << "," << run_number << "," << config.num_apps << "," << config.num_connections << ","
       << config.num_messages << "," << config.message_size_kb << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control - start).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_info - after_control).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_callbacks - after_info).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_receives - after_callbacks).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_cleanup - after_receives).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_send - after_cleanup).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_send - after_receives).count() << ","
       << min_get_time << "," << max_get_time << "," << average_get_time << "," << min_cb_time << "," << max_cb_time
       << "," << average_cb_time << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_send - start).count() << std::endl;

    receivers.clear();
  }
};

struct SenderTest
{
  struct SenderInfo
  {
    std::shared_ptr<SenderConcept<Data>> sender;
    std::atomic<size_t> msgs_sent{ 0 };
    std::unique_ptr<std::thread> send_thread;
    std::chrono::milliseconds get_sender_time;
  };

  std::unordered_map<size_t, std::shared_ptr<SenderInfo>> senders;
  TestConfig config;

  explicit SenderTest(TestConfig c)
    : config(c)
  {
  }

  void send(size_t run_number)
  {
    auto start = std::chrono::steady_clock::now();
    auto control_receiver = dunedaq::get_iom_receiver<Control>("conn_" + std::to_string(config.my_id) + "_control");
    std::atomic<bool> ready_msg_received{ false };
    std::atomic<bool> end_msg_received{ false };
    auto control_callback = [=, &end_msg_received, &ready_msg_received](Control& msg) {
      TLOG_DEBUG(5) << "Received Control message Ready: " << std::boolalpha << msg.ready_to_receive
                    << ", Done: " << msg.done_receiving << " from ID " << msg.receiver_id << " for run "
                    << msg.run_number;

      if (msg.receiver_id == config.my_id && msg.run_number == run_number) {
        end_msg_received = msg.done_receiving;
        ready_msg_received = msg.ready_to_receive;
      }
    };
    control_receiver->add_callback(control_callback);
    auto after_control = std::chrono::steady_clock::now();

    for (size_t conn_id = 0; conn_id < config.num_connections; ++conn_id) {
      auto info = std::make_shared<SenderInfo>();
      senders[conn_id] = info;
    }
    auto after_counters = std::chrono::steady_clock::now();
    std::for_each(std::execution::par_unseq,
                  std::begin(senders),
                  std::end(senders),
                  [&](std::pair<size_t, std::shared_ptr<SenderInfo>> info_pair) {
                    auto conn_id = info_pair.first;
                    auto info = info_pair.second;
                    auto before_sender = std::chrono::steady_clock::now();
                    info->sender = dunedaq::get_iom_sender<Data>(config.get_connection_name(conn_id));
                    auto after_sender = std::chrono::steady_clock::now();
                    info->get_sender_time =
                      std::chrono::duration_cast<std::chrono::milliseconds>(after_sender - before_sender);
                  });

    auto after_senders = std::chrono::steady_clock::now();

    for (size_t conn_id = 0; conn_id < config.num_connections; ++conn_id) {
      auto info = senders[conn_id];
      info->send_thread.reset(new std::thread([=, &ready_msg_received]() {
        while (!ready_msg_received.load()) {
          std::this_thread::sleep_for(10ms);
        }
        for (size_t ii = 0; ii < config.num_messages; ++ii) {

          TLOG_DEBUG(4) << "Sending message " << ii << " with size " << config.message_size_kb * 1024
                        << " bytes to connection " << config.get_connection_name(conn_id);

          Data d(ii, config.message_size_kb * 1024);
          info->sender->send(std::move(d), Sender::s_block);
          info->msgs_sent++;
        }
      }));
    }
    auto after_send_start = std::chrono::steady_clock::now();

    bool all_done = false;
    while (!all_done) {
      size_t senders_done = 0;
      for (auto& sndr : senders) {
        if (sndr.second->msgs_sent.load() == config.num_messages)
          senders_done++;
      }
      all_done = senders_done == config.num_connections;
    }
    auto after_sends = std::chrono::steady_clock::now();

    for (auto& sender : senders) {
      sender.second->send_thread->join();
      sender.second->send_thread.reset(nullptr);
    }
    auto after_join = std::chrono::steady_clock::now();

    while (end_msg_received.load() == false) {
      std::this_thread::sleep_for(100ms);
    }
    control_receiver->remove_callback();
    auto after_control_recvd = std::chrono::steady_clock::now();

    int min_get_time = std::numeric_limits<int>::max();
    int max_get_time = 0;
    int sum_get_time = 0;
    for (auto& info : senders) {
      auto get_time = info.second->get_sender_time.count();
      if (get_time > max_get_time)
        max_get_time = get_time;
      if (get_time < min_get_time)
        min_get_time = get_time;
      sum_get_time += get_time;
    }
    double average_get_time = static_cast<double>(sum_get_time) / static_cast<double>(config.num_connections);

    std::string info_file_name = config.info_file_base + "_sender.csv";

    struct stat buffer;
    bool file_exists = stat(info_file_name.c_str(), &buffer) == 0;

    std::ofstream of(info_file_name, std::ios::app);

    if (!file_exists) {
      of << "APP_TYPE,APP_D,RUN,N_APPS,N_CONN,N_MESS,MESS_SZ_KB,CONTROL_MS,COUNTER_MS,DATA_MS,THREAD_MS,COMPLETE_MS,"
            "JOIN_MS,ACK_MS,"
            "SEND_MS,TOTAL_MS,MIN_SENDER_MS,MAX_SENDER_MS,AVG_SENDER_MS"
         << std::endl;
    }

    of << "S"
       << "," << config.my_id << "," << run_number << "," << config.num_apps << "," << config.num_connections << ","
       << config.num_messages << "," << config.message_size_kb << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control - start).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_counters - after_control).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_senders - after_counters).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_send_start - after_senders).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_sends - after_send_start).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_join - after_sends).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_recvd - after_join).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_recvd - after_senders).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_recvd - start).count() << ","
       << min_get_time << "," << max_get_time << "," << average_get_time << std::endl;

    senders.clear();
  }
};

}
// Must be in dunedaq namespace only
DUNE_DAQ_SERIALIZABLE(iomanager::Data, "data_t");
DUNE_DAQ_SERIALIZABLE(iomanager::Control, "control_t");
}

int
main(int argc, char* argv[])
{
  dunedaq::logging::Logging::setup();
  dunedaq::iomanager::TestConfig config;

  bool help_requested = false;
  namespace po = boost::program_options;
  po::options_description desc("Program to test IOManager load with many connections");
  desc.add_options()("use_connectivity_service,c",
                     po::bool_switch(&config.use_connectivity_service),
                     "enable the ConnectivityService in IOManager")(
    "num_apps,N", po::value<size_t>(&config.num_apps), "Number of applications to start")(
    "num_connections,n",
    po::value<size_t>(&config.num_connections),
    "Number of connections to register and use in each app pair")(
    "port,p", po::value<int>(&config.port), "port to connect to on configuration server")(
    "server,s", po::value<std::string>(&config.server), "Configuration server to connect to")(
    "num_messages,m", po::value<size_t>(&config.num_messages), "Number of messages to send on each connection")(
    "message_size_kb,z", po::value<size_t>(&config.message_size_kb), "Size of each message, in KB")(
    "num_runs,r", po::value<size_t>(&config.num_runs), "Number of times to clear the sender and send all messages")(
    "publish_interval,i",
    po::value<int>(&config.publish_interval),
    "Interval, in ms, for ConfigClient to re-publish connection info")(
    "output_file_base,o",
    po::value<std::string>(&config.info_file_base),
    "Base name for output info file (will have _sender.csv or _receiver.csv appended)")(
    "help,h", po::bool_switch(&help_requested), "Print this help message");

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

  if (config.use_connectivity_service) {
    TLOG() << "Setting Connectivity Service Environment variables";
    config.configure_connsvc();
  }

  auto startup_time = std::chrono::steady_clock::now();
  bool is_receiver = false;
  bool is_sender = false;
  std::vector<pid_t> forked_pids;
  for (size_t ii = 0; ii < config.num_apps; ++ii) {
    auto pid = fork();
    if (pid == 0) { // child

      forked_pids.clear();
      config.my_id = ii;

      auto sender_pid = fork();
      if (sender_pid == 0) { // Sender
        is_receiver = false;
        is_sender = true;
      } else {
        forked_pids.push_back(sender_pid);
        is_receiver = true;
      }

      TLOG() << "STARTUP: I am a " << (is_receiver ? "Receiver" : "Sender") << " process with ID " << config.my_id;
      break;
    } else {
      forked_pids.push_back(pid);
    }
  }

  if (is_sender || is_receiver) {
    std::this_thread::sleep_until(startup_time + 2s);

    TLOG() << (is_receiver ? "Receiver " : "Sender ") << config.my_id << ": "
           << "Configuring IOManager";
    config.configure_iomanager(is_receiver);

    for (size_t run = 0; run < config.num_runs; ++run) {
      TLOG() << (is_receiver ? "Receiver " : "Sender ") << config.my_id << ": "
             << "Starting test run " << run;
      if (is_receiver) {
        dunedaq::iomanager::ReceiverTest receiver(config);
        receiver.receive(run);
      } else {
        dunedaq::iomanager::SenderTest sender(config);
        sender.send(run);
      }
      TLOG() << (is_receiver ? "Receiver " : "Sender ") << config.my_id << ": "
             << "Test run " << run << " complete.";
    }

    TLOG() << (is_receiver ? "Receiver " : "Sender ") << config.my_id << ": "
           << "Cleaning up";
    dunedaq::iomanager::IOManager::get()->reset();
    TLOG() << (is_receiver ? "Receiver " : "Sender ") << config.my_id << ": "
           << "DONE";
  }

  if (forked_pids.size() > 0) {
    TLOG() << (is_receiver ? "Receiver " : "Sender ") << config.my_id << ": "
           << "Waiting for forked PIDs";

    for (auto& pid : forked_pids) {
      siginfo_t status;
      auto sts = waitid(P_PID, pid, &status, WEXITED);

      TLOG_DEBUG(6) << "Forked process " << pid << " exited with status " << status.si_status << " (wait status " << sts
                    << ")";
    }
  }
};