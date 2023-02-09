/**
 * @file iomanager_stress_test_pubsub.cxx
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
  size_t publisher_id;
  size_t group_id;
  size_t conn_id;
  std::vector<uint8_t> contents;

  Data() = default;
  Data(size_t seq, size_t publisher, size_t group, size_t conn, size_t size)
    : seq_number(seq)
    , publisher_id(publisher)
    , group_id(group)
    , conn_id(conn)
    , contents(size)
  {
  }
  virtual ~Data() = default;
  Data(Data const&) = default;
  Data& operator=(Data const&) = default;
  Data(Data&&) = default;
  Data& operator=(Data&&) = default;

  DUNE_DAQ_SERIALIZE(Data, seq_number, publisher_id, group_id, conn_id, contents);
};

struct QuotaReached
{
  size_t sender_id;
  int group_id;
  size_t conn_id;
  size_t run_number;

  QuotaReached() = default;
  QuotaReached(size_t sender, int group, size_t conn, size_t run)
    : sender_id(sender)
    , group_id(group)
    , conn_id(conn)
    , run_number(run)
  {
  }

  DUNE_DAQ_SERIALIZE(QuotaReached, sender_id, group_id, conn_id, run_number);
};

struct TestConfig
{

  bool use_connectivity_service = false;
  int port = 5000;
  std::string server = "localhost";
  std::string info_file_base = "iom_stress_test_pubsub";
  size_t num_apps = 1;
  size_t num_connections_per_group = 1;
  size_t num_groups = 1;
  size_t num_messages = 1;
  size_t message_size_kb = 1024;
  size_t num_runs = 1;
  size_t my_id = 0;
  size_t send_interval_ms = 100;
  int publish_interval = 1000;

  void configure_connsvc()
  {
    setenv("CONNECTION_SERVER", server.c_str(), 1);
    setenv("CONNECTION_PORT", std::to_string(port).c_str(), 1);
  }

  std::string get_connection_name(size_t app_id, size_t group_id, size_t conn_id)
  {
    std::stringstream ss;
    ss << "conn_A" << app_id << "_G" << group_id << "_C" << conn_id << "_";
    return ss.str();
  }
  std::string get_group_connection_name(size_t app_id, size_t group_id)
  {

    std::stringstream ss;
    ss << "conn_A" << app_id << "_G" << group_id << "_.*";
    return ss.str();
  }

  std::string get_connection_ip(size_t app_id, size_t group_id, size_t conn_id)
  {
    assert(num_apps < 253);
    assert(num_groups < 253);
    assert(num_connections_per_group < 252);

    int first_byte = conn_id + 2;   // 2-254
    int second_byte = group_id + 1; // 1-254
    int third_byte = app_id + 1;    // 1 - 254

    std::string conn_addr = "tcp://127." + std::to_string(third_byte) + "." + std::to_string(second_byte) + "." +
                            std::to_string(first_byte) + ":15500";

    return conn_addr;
  }

  std::string get_subscriber_quota_name() { return get_subscriber_quota_name(my_id); }
  std::string get_subscriber_quota_name(size_t id) { return "conn_quota_" + std::to_string(id); }
  std::string get_publisher_quota_name() { return "conn_quota_.*"; }

  void configure_iomanager(bool is_publisher)
  {
    setenv("DUNEDAQ_PARTITION", "iomanager_stress_test", 0);

    Queues_t queues;
    Connections_t connections;

    if (is_publisher || !use_connectivity_service) {
      for (size_t group = 0; group < num_groups; ++group) {
        for (size_t conn = 0; conn < num_connections_per_group; ++conn) {
          auto conn_addr = get_connection_ip(my_id, group, conn);
          TLOG_DEBUG(1) << "Adding connection with id " << get_connection_name(my_id, group, conn) << " and address "
                        << conn_addr;

          connections.emplace_back(Connection{
            ConnectionId{ get_connection_name(my_id, group, conn), "data_t" }, conn_addr, ConnectionType::kPubSub });
        }
      }
    }

    if (!is_publisher) {
      auto port = 13000 + my_id;
      std::string conn_addr = "tcp://127.0.0.1:" + std::to_string(port);
      TLOG_DEBUG(1) << "Adding control connection " << get_subscriber_quota_name() << " with address " << conn_addr;

      connections.emplace_back(
        Connection{ ConnectionId{ get_subscriber_quota_name(), "quota_t" }, conn_addr, ConnectionType::kPubSub });
    }

    if (is_publisher && !use_connectivity_service) {
      for (size_t sub = 0; sub < num_apps; ++sub) {
        auto port = 13000 + sub;
        std::string conn_addr = "tcp://127.0.0.1:" + std::to_string(port);
        TLOG_DEBUG(1) << "Adding control connection " << get_subscriber_quota_name(sub) << " with address "
                      << conn_addr;

        connections.emplace_back(
          Connection{ ConnectionId{ get_subscriber_quota_name(sub), "quota_t" }, conn_addr, ConnectionType::kPubSub });
      }
    }

    IOManager::get()->configure(
      queues, connections, use_connectivity_service, std::chrono::milliseconds(publish_interval));
  }
};
struct SubscriberTest
{
  struct SubscriberInfo
  {
    size_t group_id;
    size_t conn_id;
    bool is_group_subscriber;
    std::unordered_map<size_t, size_t> last_sequence_received{ 0 };
    std::atomic<size_t> msgs_received{ 0 };
    std::atomic<size_t> msgs_with_error{ 0 };
    std::chrono::milliseconds get_receiver_time;
    std::chrono::milliseconds add_callback_time;
    std::atomic<bool> complete{ false };

    SubscriberInfo(size_t group, size_t conn)
      : group_id(group)
      , conn_id(conn)
      , is_group_subscriber(false)
    {
    }
    SubscriberInfo(size_t group)
      : group_id(group)
      , conn_id(0)
      , is_group_subscriber(true)
    {
    }

    std::string get_connection_name(TestConfig& config)
    {
      if (is_group_subscriber) {
        return config.get_group_connection_name(config.my_id, group_id);
      }
      return config.get_connection_name(config.my_id, group_id, conn_id);
    }
  };
  std::vector<std::shared_ptr<SubscriberInfo>> subscribers;
  TestConfig config;

  explicit SubscriberTest(TestConfig c)
    : config(c)
  {
  }

  void receive(size_t run_number)
  {
    TLOG_DEBUG(5) << "Getting QuotaReached sender";
    auto start = std::chrono::steady_clock::now();
    auto quota_sender = dunedaq::get_iom_sender<QuotaReached>(config.get_subscriber_quota_name());
    auto after_control = std::chrono::steady_clock::now();

    TLOG_DEBUG(5) << "Setting up SubscriberInfo objects";
    for (size_t group = 0; group < config.num_groups; ++group) {
      subscribers.push_back(std::make_shared<SubscriberInfo>(group));
      for (size_t conn = 0; conn < config.num_connections_per_group; ++conn) {
        subscribers.push_back(std::make_shared<SubscriberInfo>(group, conn));
      }
    }

    std::atomic<std::chrono::steady_clock::time_point> last_received = std::chrono::steady_clock::now();
    TLOG_DEBUG(5) << "Adding callbacks for each subscriber";
    auto after_info = std::chrono::steady_clock::now();
    std::for_each(std::execution::par_unseq,
                  std::begin(subscribers),
                  std::end(subscribers),
                  [=, &last_received](std::shared_ptr<SubscriberInfo> info) {
                    auto recv_proc = [=, &last_received](Data& msg) {
                      TLOG_DEBUG(3) << "Received message " << msg.seq_number << " with size " << msg.contents.size()
                                    << " bytes from connection "
                                    << config.get_connection_name(msg.publisher_id, msg.group_id, msg.conn_id) << " at "
                                    << info->get_connection_name(config);

                      if (msg.contents.size() != config.message_size_kb * 1024 ||
                          msg.seq_number != info->last_sequence_received[msg.conn_id] + 1) {
                        info->msgs_with_error++;
                      }
                      info->last_sequence_received[msg.conn_id] = msg.seq_number;
                      info->msgs_received++;
                      last_received = std::chrono::steady_clock::now();

                      if (info->msgs_received >= config.num_messages && !info->is_group_subscriber) {
                        TLOG_DEBUG(3) << "Complete condition reached, sending QuotaReached message for "
                                      << info->get_connection_name(config);
                        QuotaReached q(config.my_id, info->group_id, info->conn_id, run_number);
                        quota_sender->send(std::move(q), Sender::s_block);
                        info->complete = true;
                      }
                    };

                    auto before_receiver = std::chrono::steady_clock::now();
                    auto receiver = dunedaq::get_iom_receiver<Data>(info->get_connection_name(config));
                    auto after_receiver = std::chrono::steady_clock::now();
                    receiver->add_callback(recv_proc);
                    auto after_callback = std::chrono::steady_clock::now();
                    info->get_receiver_time =
                      std::chrono::duration_cast<std::chrono::milliseconds>(after_receiver - before_receiver);
                    info->add_callback_time =
                      std::chrono::duration_cast<std::chrono::milliseconds>(after_callback - after_receiver);
                  });
    auto after_callbacks = std::chrono::steady_clock::now();

    TLOG_DEBUG(5) << "Starting wait loop for receives to complete";
    bool all_done = false;
    while (!all_done) {
      size_t recvrs_done = 0;
      for (auto& sub : subscribers) {
        if (sub->complete.load())
          recvrs_done++;
      }
      TLOG_DEBUG(6) << "Done: " << recvrs_done
                    << ", expected: " << config.num_groups * config.num_connections_per_group;
      all_done = recvrs_done >= config.num_groups * config.num_connections_per_group;
      if (!all_done)
        std::this_thread::sleep_for(1ms);
    }
    auto after_receives = std::chrono::steady_clock::now();

    while (
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - last_received.load())
        .count() < 500) {
      QuotaReached q(config.my_id, -1, 0, run_number);
      quota_sender->send(std::move(q), Sender::s_block);
      std::this_thread::sleep_for(100ms);
    }

    TLOG_DEBUG(5) << "Removing callbacks";
    for (auto& info : subscribers) {
      auto receiver = dunedaq::get_iom_receiver<Data>(info->get_connection_name(config));
      receiver->remove_callback();
    }
    auto after_cleanup = std::chrono::steady_clock::now();

    TLOG_DEBUG(5) << "Calculating statistics";
    int min_get_time = std::numeric_limits<int>::max();
    int max_get_time = 0;
    int sum_get_time = 0;
    int min_cb_time = std::numeric_limits<int>::max();
    int max_cb_time = 0;
    int sum_cb_time = 0;
    for (auto& info : subscribers) {
      TLOG_DEBUG(6) << "Received " << info->msgs_received << " messages at " << info->get_connection_name(config)
                    << " (" << info->msgs_with_error << " size/sequence errors)";
      auto get_time = info->get_receiver_time.count();
      auto cb_time = info->add_callback_time.count();
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
    double average_get_time = static_cast<double>(sum_get_time) / static_cast<double>(subscribers.size());
    double average_cb_time = static_cast<double>(sum_cb_time) / static_cast<double>(subscribers.size());

    std::string info_file_name = config.info_file_base + "_receiver.csv";

    TLOG_DEBUG(5) << "Writing to output CSV file " << info_file_name;
    struct stat buffer;
    bool file_exists = stat(info_file_name.c_str(), &buffer) == 0;

    std::ofstream of(info_file_name, std::ios::app);

    if (!file_exists) {
      of << "APP_TYPE,APP_ID,RUN,N_APPS,N_GROUPS,N_CONN,N_MESS,MESS_SZ_KB,CONTROL_MS,COUNTERS_MS,ADD_CALLBACKS_MS,"
            "RECV_COMPLETE_MS,RM_CALLBACKS_MS,TOTAL_MS,MIN_GET_RECVR_TIME,MAX_GET_RECVR_TIME,AVG_GET_"
            "RECVR_TIME,MIN_ADD_CB_TIME,MAX_ADD_CB_TIME,AVG_ADD_CB_TIME"
         << std::endl;
    }

    of << "R"
       << "," << config.my_id << "," << run_number << "," << config.num_apps << "," << config.num_groups << ","
       << config.num_connections_per_group << "," << config.num_messages << "," << config.message_size_kb << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control - start).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_info - after_control).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_callbacks - after_info).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_receives - after_callbacks).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_cleanup - after_receives).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_cleanup - start).count() << "," << min_get_time
       << "," << max_get_time << "," << average_get_time << "," << min_cb_time << "," << max_cb_time << ","
       << average_cb_time << std::endl;

    subscribers.clear();
    TLOG_DEBUG(5) << "receive() done";
  }
};

struct PublisherTest
{
  struct PublisherInfo
  {
    size_t conn_id;
    size_t group_id;
    size_t messages_sent{ 0 };
    std::shared_ptr<SenderConcept<Data>> sender;
    std::unique_ptr<std::thread> send_thread;
    std::chrono::milliseconds get_sender_time;

    PublisherInfo(size_t group, size_t conn)
      : conn_id(conn)
      , group_id(group)
    {
    }
  };

  std::vector<std::shared_ptr<PublisherInfo>> publishers;
  TestConfig config;

  explicit PublisherTest(TestConfig c)
    : config(c)
  {
  }

  void send(size_t run_number, pid_t subscriber_pid)
  {
    TLOG_DEBUG(7) << "Getting QuotaReached receiver and adding callback";
    auto start = std::chrono::steady_clock::now();
    auto quota_receiver = dunedaq::get_iom_receiver<QuotaReached>(config.get_publisher_quota_name());
    std::unordered_map<int, std::set<size_t>> completed_receiver_tracking;
    std::mutex tracking_mutex;
    auto quota_callback = [=, &completed_receiver_tracking, &tracking_mutex](QuotaReached& msg) {
      TLOG_DEBUG(4) << "Received QuotaReached message from app " << msg.sender_id << ", group " << msg.group_id
                    << ", conn " << msg.conn_id << " for run " << msg.run_number;

      if (msg.run_number == run_number && msg.sender_id == config.my_id) {
        TLOG_DEBUG(4) << "Informing sender for " << config.get_connection_name(config.my_id, msg.group_id, msg.conn_id)
                      << " that it can stop";
        std::lock_guard<std::mutex> lk(tracking_mutex);
        completed_receiver_tracking[msg.group_id].insert(msg.conn_id);
      }
    };
    quota_receiver->add_callback(quota_callback);
    auto after_control = std::chrono::steady_clock::now();

    auto check_subscriber = [subscriber_pid]() {
      auto ret = kill(subscriber_pid, 0);
      return ret == 0;
    };

    TLOG_DEBUG(7) << "Setting up PublisherInfo objects";
    for (size_t group = 0; group < config.num_groups; ++group) {
      for (size_t conn = 0; conn < config.num_connections_per_group; ++conn) {
        auto info = std::make_shared<PublisherInfo>(group, conn);
        publishers.push_back(info);
      }
    }
    auto after_counters = std::chrono::steady_clock::now();

    TLOG_DEBUG(7) << "Getting publisher objects for each connection";
    std::for_each(std::execution::par_unseq,
                  std::begin(publishers),
                  std::end(publishers),
                  [=](std::shared_ptr<PublisherInfo> info) {
                    auto before_sender = std::chrono::steady_clock::now();
                    info->sender = dunedaq::get_iom_sender<Data>(
                      config.get_connection_name(config.my_id, info->group_id, info->conn_id));
                    auto after_sender = std::chrono::steady_clock::now();
                    info->get_sender_time =
                      std::chrono::duration_cast<std::chrono::milliseconds>(after_sender - before_sender);
                  });

    auto after_senders = std::chrono::steady_clock::now();

    TLOG_DEBUG(7) << "Starting publish threads";
    std::for_each(
      std::execution::par_unseq,
      std::begin(publishers),
      std::end(publishers),
      [=, &completed_receiver_tracking, &tracking_mutex](std::shared_ptr<PublisherInfo> info) {
        info->send_thread.reset(new std::thread([=, &completed_receiver_tracking, &tracking_mutex]() {
          bool complete_received = false;
          while (!complete_received) {
            TLOG_DEBUG(4) << "Sending message " << info->messages_sent << " with size " << config.message_size_kb * 1024
                          << " bytes to connection "
                          << config.get_connection_name(config.my_id, info->group_id, info->conn_id);

            Data d(info->messages_sent, config.my_id, info->group_id, info->conn_id, config.message_size_kb * 1024);
            info->sender->try_send(std::move(d), std::chrono::milliseconds(config.send_interval_ms));
            ++info->messages_sent;
            {
              std::lock_guard<std::mutex> lk(tracking_mutex);
              if ((completed_receiver_tracking.count(info->group_id) &&
                   completed_receiver_tracking[info->group_id].count(info->conn_id)) ||
                  completed_receiver_tracking.count(-1)) {
                complete_received = true;
              }
            }

            if (!check_subscriber()) {
              TLOG_DEBUG(7) << "Subscriber app has gone away, but I didn't receive a QuotaReached message!";
              complete_received = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(config.send_interval_ms));
          }
        }));
      });
    auto after_send_start = std::chrono::steady_clock::now();

    TLOG_DEBUG(7) << "Joining send threads";
    for (auto& sender : publishers) {
      sender->send_thread->join();
      sender->send_thread.reset(nullptr);
    }
    auto after_join = std::chrono::steady_clock::now();

    TLOG_DEBUG(7) << "Removing QuotaReached callback";
    quota_receiver->remove_callback();
    auto after_control_recvd = std::chrono::steady_clock::now();

    TLOG_DEBUG(7) << "Calculating statistics";
    int min_get_time = std::numeric_limits<int>::max();
    int max_get_time = 0;
    int sum_get_time = 0;
    for (auto& info : publishers) {
      TLOG_DEBUG(8) << "Published " << info->messages_sent << " messages to connection "
                    << config.get_connection_name(config.my_id, info->group_id, info->conn_id);
      auto get_time = info->get_sender_time.count();
      if (get_time > max_get_time)
        max_get_time = get_time;
      if (get_time < min_get_time)
        min_get_time = get_time;
      sum_get_time += get_time;
    }
    double average_get_time = static_cast<double>(sum_get_time) / static_cast<double>(publishers.size());

    std::string info_file_name = config.info_file_base + "_sender.csv";
    TLOG_DEBUG(7) << "Writing to output CSV file " << info_file_name;

    struct stat buffer;
    bool file_exists = stat(info_file_name.c_str(), &buffer) == 0;

    std::ofstream of(info_file_name, std::ios::app);

    if (!file_exists) {
      of << "APP_TYPE,APP_D,RUN,N_APPS,N_GROUPS,N_CONN,N_MESS,MESS_SZ_KB,CONTROL_MS,COUNTER_MS,DATA_MS,THREAD_MS,"
            "COMPLETE_MS,JOIN_MS,ACK_MS,SEND_MS,TOTAL_MS,MIN_SENDER_MS,MAX_SENDER_MS,AVG_SENDER_MS"
         << std::endl;
    }

    of << "S"
       << "," << config.my_id << "," << run_number << "," << config.num_apps << "," << config.num_groups << ","
       << config.num_connections_per_group << "," << config.num_messages << "," << config.message_size_kb << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control - start).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_counters - after_control).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_senders - after_counters).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_send_start - after_senders).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_join - after_send_start).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_recvd - after_join).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_recvd - after_senders).count() << ","
       << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_recvd - start).count() << ","
       << min_get_time << "," << max_get_time << "," << average_get_time << std::endl;

    publishers.clear();

    TLOG_DEBUG(7) << "send() done";
  }
};

}
// Must be in dunedaq namespace only
DUNE_DAQ_SERIALIZABLE(iomanager::Data, "data_t");
DUNE_DAQ_SERIALIZABLE(iomanager::QuotaReached, "quota_t");
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
    "num_groups,g", po::value<size_t>(&config.num_groups), "Number of connection groups")(
    "num_connections,n",
    po::value<size_t>(&config.num_connections_per_group),
    "Number of connections to register and use in each group")(
    "port,p", po::value<int>(&config.port), "port to connect to on configuration server")(
    "server,s", po::value<std::string>(&config.server), "Configuration server to connect to")(
    "num_messages,m", po::value<size_t>(&config.num_messages), "Number of messages to send on each connection")(
    "message_size_kb,z", po::value<size_t>(&config.message_size_kb), "Size of each message, in KB")(
    "num_runs,r", po::value<size_t>(&config.num_runs), "Number of times to clear the sender and send all messages")(
    "publish_interval,i",
    po::value<int>(&config.publish_interval),
    "Interval, in ms, for ConfigClient to re-publish connection info")(
    "send_interval,I", po::value<size_t>(&config.send_interval_ms), "Interval, in ms, for Publishers to send messages")(
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
  bool is_subscriber = false;
  bool is_publisher = false;
  std::vector<pid_t> forked_pids;
  for (size_t ii = 0; ii < config.num_apps; ++ii) {
    auto pid = fork();
    if (pid == 0) { // child

      forked_pids.clear();
      config.my_id = ii;

      auto subscriber_pid = fork();
      if (subscriber_pid == 0) {
        is_subscriber = true;
        is_publisher = false;
      } else {
        is_publisher = true;
        forked_pids.push_back(subscriber_pid);
      }
      TLOG() << "STARTUP: I am a " << (is_subscriber ? "Subscriber" : "Publisher") << " process with ID "
             << config.my_id;
      break;
    } else {
      forked_pids.push_back(pid);
    }
  }

  if (is_subscriber || is_publisher) {
    std::this_thread::sleep_until(startup_time + 2s);

    TLOG() << (is_subscriber ? "Subscriber " : "Publisher ") << config.my_id << ": "
           << "Configuring IOManager";
    config.configure_iomanager(is_publisher);

    auto subscriber = std::make_unique<dunedaq::iomanager::SubscriberTest>(config);
    auto publisher = std::make_unique<dunedaq::iomanager::PublisherTest>(config);

    for (size_t run = 0; run < config.num_runs; ++run) {
      TLOG() << (is_subscriber ? "Subscriber " : "Publisher ") << config.my_id << ": "
             << "Starting test run " << run;
      if (is_subscriber) {
        subscriber->receive(run);
      } else {
        publisher->send(run, forked_pids[0]);
      }
      TLOG() << (is_subscriber ? "Subscriber " : "Publisher ") << config.my_id << ": "
             << "Test run " << run << " complete.";
    }

    TLOG() << (is_subscriber ? "Subscriber " : "Publisher ") << config.my_id << ": "
           << "Cleaning up";
    subscriber.reset(nullptr);
    publisher.reset(nullptr);

    dunedaq::iomanager::IOManager::get()->reset();
    TLOG() << (is_subscriber ? "Subscriber " : "Publisher ") << config.my_id << ": "
           << "DONE";
  }

  if (forked_pids.size() > 0) {
    TLOG() << "Waiting for forked PIDs";

    for (auto& pid : forked_pids) {
      siginfo_t status;
      auto sts = waitid(P_PID, pid, &status, WEXITED);

      TLOG_DEBUG(6) << "Forked process " << pid << " exited with status " << status.si_status << " (wait status " << sts
                    << ")";
    }
  }
};