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
  bool done_receiving;

  Control() = default;
  Control(size_t id, size_t run, bool done)
    : receiver_id(id)
    , run_number(run)
    , done_receiving(done)
  {
  }

  DUNE_DAQ_SERIALIZE(Control, receiver_id, run_number, done_receiving);
};

struct TestConfig
{

  bool use_connectivity_service = false;
  int port = 5000;
  std::string server = "localhost";
  std::string info_file_base = "iom_stress_test";
  size_t num_apps = 10;
  size_t num_connections = 10;
  size_t num_messages = 100;
  size_t message_size_kb = 10;
  size_t num_runs = 2;
  size_t my_id = 0;
  int publish_interval = 1000;
  bool verbose = false;

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
        if (verbose) {
          TLOG() << "Adding connection with id " << get_connection_name(ii) << " and address " << conn_addr;
        }
        connections.emplace_back(
          Connection{ ConnectionId{ get_connection_name(ii), "data_t" }, conn_addr, ConnectionType::kSendRecv });
      }

      int port = 25500 + my_id;
      std::string conn_addr = "tcp://127.0.0.1:" + std::to_string(port);
      if (verbose) {
        TLOG() << "Adding control connection conn_control_" << my_id << " with address " << conn_addr;
      }
      connections.emplace_back(Connection{
        ConnectionId{ "conn_control_" + std::to_string(my_id), "control_t" }, conn_addr, ConnectionType::kPubSub });
    }
    IOManager::get()->configure(queues, connections, use_connectivity_service /*, publish_interval*/);
  }
};
struct ReceiverTest 
{
  struct ReceiverInfo
  {
    size_t last_sequence_received{ 0 };
    std::atomic<size_t> msgs_received{ 0 };
    std::atomic<size_t> msgs_with_error{ 0 };
    std::chrono::steady_clock::time_point first_receive;
    std::chrono::steady_clock::time_point last_receive;
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
    auto control_sender = dunedaq::get_iom_sender<Control>( "conn_control_" + std::to_string(config.my_id));
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
                      if (config.verbose) {
                        TLOG() << "Received message " << msg.seq_number << " with size " << msg.contents.size()
                               << " bytes from connection " << conn_id;
                      }
                      if (!info->first_received) {
                        info->first_received = true;
                        info->first_receive = std::chrono::steady_clock::now();
                      }
                      if (msg.contents.size() != config.message_size_kb * 1024 ||
                          msg.seq_number != info->last_sequence_received + 1) {
                        info->msgs_with_error++;
                      }
                      info->last_sequence_received = msg.seq_number;
                      info->last_receive = std::chrono::steady_clock::now();
                      info->msgs_received++;
                    };

                    auto receiver = dunedaq::get_iom_receiver<Data>(config.get_connection_name(conn_id));
                    receiver->add_callback(recv_proc);
                  });
    auto after_callbacks = std::chrono::steady_clock::now();

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

    Control msg(config.my_id, run_number, true);

    control_sender->send(std::move(msg), Sender::s_block);

    auto after_control_send = std::chrono::steady_clock::now();

    int min_time = std::numeric_limits<int>::max();
    int max_time = 0;
    int sum_time = 0;
    for (auto& info : receivers) {
      auto time =
        std::chrono::duration_cast<std::chrono::milliseconds>(info.second->last_receive - info.second->first_receive)
          .count();
      if (time > max_time)
        max_time = time;
      if (time < min_time)
        min_time = time;
      sum_time += time;
    }
    double average_time = static_cast<double>(sum_time) / static_cast<double>(config.num_connections);

    std::string info_file_name = config.info_file_base + "_receiver.csv";

    struct stat buffer;
    bool file_exists = stat(info_file_name.c_str(), &buffer) == 0;

    std::ofstream of(info_file_name, std::ios::app);

    if (!file_exists) {
      of << "APP_TYPE,APP_ID,RUN,N_CONN,N_MESS,MESS_SZ_KB,CONTROL_MS,COUNTERS_MS,ADD_CALLBACKS_MS,RECV_COMPLETE_MS"
            ",RM_CALLBACKS_MS,SEND_COMPMSG_MS,RECV_MS,MIN_RECV_MS,MAX_RECV_MS,AVG_RECV_MS,TOTAL_MS"
         << std::endl;
    }

    of << "R"
       << "," << config.my_id << "," << run_number << "," << config.num_connections << "," << config.num_messages << ","
       << config.message_size_kb << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control - start).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_info - after_control).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_callbacks - after_info).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_receives - after_callbacks).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_cleanup - after_receives).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_send - after_cleanup).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_send - after_receives).count() << ","
       << min_time << "," << max_time << "," << average_time << ","
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
    auto control_receiver = dunedaq::get_iom_receiver<Control>( "conn_control_" + std::to_string(config.my_id));
    auto after_control = std::chrono::steady_clock::now();

    for (size_t conn_id = 0; conn_id < config.num_connections; ++conn_id) {
      auto info = std::make_shared<SenderInfo>();
      info->sender = dunedaq::get_iom_sender<Data>(config.get_connection_name(conn_id));
      senders[conn_id] = info;
    }
    auto after_senders = std::chrono::steady_clock::now();

    for (size_t conn_id = 0; conn_id < config.num_connections; ++conn_id) {
      auto info = senders[conn_id];
      info->send_thread.reset(new std::thread([=]() {
        for (size_t ii = 0; ii < config.num_messages; ++ii) {

          if (config.verbose) {
            TLOG() << "Sending message " << ii << " with size " << config.message_size_kb * 1024
                   << " bytes to connection " << conn_id;
          }

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

    auto controlMsg = control_receiver->receive(Receiver::s_block);
    auto after_control_recvd = std::chrono::steady_clock::now();
    if (config.verbose) {
      TLOG() << "Received control message indicating completion of reception: " << controlMsg.done_receiving;
    }

    std::string info_file_name = config.info_file_base + "_sender.csv";

    struct stat buffer;
    bool file_exists = stat(info_file_name.c_str(), &buffer) == 0;

    std::ofstream of(info_file_name, std::ios::app);

    if (!file_exists) {
      of << "APP_TYPE,RUN,N_CONN,N_MESS,MESS_SZ_KB,CONTROL_MS,DATA_MS,THREAD_MS,COMPLETE_MS,JOIN_MS,ACK_MS,"
            "SEND_MS,TOTAL_MS"
         << std::endl;
    }

    of << "S"
       << "," << config.my_id << "," << run_number << "," << config.num_connections << "," << config.num_messages << ","
       << config.message_size_kb << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control - start).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_senders - after_control).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_send_start - after_senders).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_sends - after_send_start).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_join - after_sends).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_recvd - after_join).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_recvd - after_senders).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_recvd - start).count() << std::endl;

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

  auto startup_time = std::chrono::steady_clock::now();
  bool is_receiver = false;
  for (size_t ii = 1; ii < 2 * config.num_apps; ++ii) {
    auto pid = fork();
    if (pid == 0) { // child
      is_receiver = ii >= config.num_apps;
      config.my_id = is_receiver ? ii - config.num_apps : ii;
      TLOG() << "STARTUP: I am a " << (is_receiver ? "Receiver" : "Sender") << " process with ID " << config.my_id;
      break;
    }
  }

  std::this_thread::sleep_until(startup_time + 2s);

  TLOG() << "Configuring IOManager";
  config.configure_iomanager(is_receiver);

  for (size_t run = 0; run < config.num_runs; ++run) {
    TLOG() << "Starting test run " << run;
    if (is_receiver) {
      dunedaq::iomanager::ReceiverTest receiver(config);
      receiver.receive(run);
    } else {
      dunedaq::iomanager::SenderTest sender(config);
      sender.send(run);
    }
    TLOG() << "Test run " << run << " complete.";
  }

  TLOG() << "Cleaning up";
  dunedaq::iomanager::IOManager::get()->reset();
  TLOG() << "DONE";
};