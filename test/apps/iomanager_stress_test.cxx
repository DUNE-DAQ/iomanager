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
}
// Must be in dunedaq namespace only
DUNE_DAQ_SERIALIZABLE(iomanager::Data, "data_t");
DUNE_DAQ_SERIALIZABLE(iomanager::Control, "control_t");
}

void
configure_connsvc(std::string host, int port)
{
  setenv("CONNECTION_SERVER", host.c_str(), 1);
  setenv("CONNECTION_PORT", std::to_string(port).c_str(), 1);
}

void
configure_iomanager(size_t num_connections, bool use_connectivity_service, int /*publish_interval*/, bool verbose)
{
  setenv("DUNEDAQ_PARTITION", "iomanager_stress_test", 0);

  dunedaq::iomanager::Queues_t queues;
  dunedaq::iomanager::Connections_t connections;

  assert(num_connections < (1 << 23));
  for (size_t ii = 0; ii < num_connections; ++ii) {
    size_t conn_ip = ii + 1;
    int first_byte = (conn_ip << 1) & 0xFF;
    if (first_byte == 0)
      first_byte = 1;
    int second_byte = (conn_ip >> 7) & 0xFF;
    int third_byte = (conn_ip >> 15) & 0xFF;
    std::string conn_addr = "tcp://127." + std::to_string(third_byte) + "." + std::to_string(second_byte) + "." +
                            std::to_string(first_byte) + ":15500";
    if (verbose) {
      TLOG() << "Adding connection with id "
             << "conn_" << ii << " and address " << conn_addr;
    }
    connections.emplace_back(
      dunedaq::iomanager::Connection{ dunedaq::iomanager::ConnectionId{ "conn_" + std::to_string(ii), "data_t" },
                                      conn_addr,
                                      dunedaq::iomanager::ConnectionType::kSendRecv });
  }

  std::string conn_addr = "tcp://127.0.0.1:25500";
  if (verbose) {
    TLOG() << "Adding control connection conn_control with address " << conn_addr;
  }
  connections.emplace_back(
    dunedaq::iomanager::Connection{ dunedaq::iomanager::ConnectionId{ "conn_control", "control_t" },
                                    conn_addr,
                                    dunedaq::iomanager::ConnectionType::kPubSub });

  dunedaq::iomanager::IOManager::get()->configure(queues, connections, use_connectivity_service /*, publish_interval*/);
}

struct receiver_info
{
  std::shared_ptr<dunedaq::iomanager::ReceiverConcept<dunedaq::iomanager::Data>> receiver;
  size_t conn_id;
  size_t last_sequence_received{ 0 };
  std::atomic<size_t> msgs_received{ 0 };
  std::atomic<size_t> msgs_with_error{ 0 };
};

void
receive(size_t my_id,
        size_t run_number,
        size_t num_connections,
        size_t num_messages,
        size_t message_size_mb,
        bool verbose)
{
  auto control_sender = dunedaq::get_iom_sender<dunedaq::iomanager::Control>("conn_control");

  std::vector<std::shared_ptr<receiver_info>> receivers;
  for (size_t conn_id = 0; conn_id < num_connections; ++conn_id) {
    auto info = std::make_shared<receiver_info>();
    info->receiver = dunedaq::get_iom_receiver<dunedaq::iomanager::Data>("conn_" + std::to_string(conn_id));
    info->conn_id = conn_id;

    auto recv_proc = [=](dunedaq::iomanager::Data& msg) {
      if (verbose) {
        TLOG() << "Received message " << msg.seq_number << " with size " << msg.contents.size()
               << " bytes from connection " << info->conn_id;
      }
      if (msg.contents.size() != message_size_mb * 1024 * 1024 || msg.seq_number != info->last_sequence_received + 1) {
        info->msgs_with_error++;
      }
      info->last_sequence_received = msg.seq_number;
      info->msgs_received++;
    };

    info->receiver->add_callback(recv_proc);
    receivers.push_back(info);
  }

  bool all_done = false;
  while (!all_done) {
    size_t recvrs_done = 0;
    for (auto& rcvr : receivers) {
      if (rcvr->msgs_received.load() == num_messages)
        recvrs_done++;
    }
    all_done = recvrs_done == num_connections;
  }

  for (auto& recvr : receivers) {
    recvr->receiver->remove_callback();
  }

  dunedaq::iomanager::Control msg(my_id, run_number, true);

  control_sender->send(std::move(msg), dunedaq::iomanager::Sender::s_block);

  receivers.clear();
}

struct sender_info
{
  std::shared_ptr<dunedaq::iomanager::SenderConcept<dunedaq::iomanager::Data>> sender;
  size_t conn_id;
  std::atomic<size_t> msgs_sent{ 0 };
  std::unique_ptr<std::thread> send_thread;
};

void
send(size_t num_connections, size_t num_messages, size_t message_size_mb, bool verbose)
{
  auto start = std::chrono::steady_clock::now();
  auto control_receiver = dunedaq::get_iom_receiver<dunedaq::iomanager::Control>("conn_control");
  auto after_control = std::chrono::steady_clock::now();

  std::vector<std::shared_ptr<sender_info>> senders;
  for (size_t conn_id = 0; conn_id < num_connections; ++conn_id) {
    auto info = std::make_shared<sender_info>();
    info->sender = dunedaq::get_iom_sender<dunedaq::iomanager::Data>("conn_" + std::to_string(conn_id));
    info->conn_id = conn_id;
    senders.push_back(info);
  }
  auto after_senders = std::chrono::steady_clock::now();

  for (size_t conn_id = 0; conn_id < num_connections; ++conn_id) {
    auto info = senders[conn_id];
    info->send_thread.reset(new std::thread([=]() {
      for (size_t ii = 0; ii < num_messages; ++ii) {

        if (verbose) {
          TLOG() << "Sending message " << ii << " with size " << message_size_mb * 1024 * 1024
                 << " bytes to connection " << info->conn_id;
        }

        dunedaq::iomanager::Data d(ii, message_size_mb * 1024 * 1024);
        info->sender->send(std::move(d), dunedaq::iomanager::Sender::s_block);
        info->msgs_sent++;
      }
    }));
  }
  auto after_send_start = std::chrono::steady_clock::now();

  bool all_done = false;
  while (!all_done) {
    size_t senders_done = 0;
    for (auto& sndr : senders) {
      if (sndr->msgs_sent.load() == num_messages)
        senders_done++;
    }
    all_done = senders_done == num_connections;
  }
  auto after_sends = std::chrono::steady_clock::now();

  for (auto& sender : senders) {
    sender->send_thread->join();
    sender->send_thread.reset(nullptr);
  }
  auto after_join = std::chrono::steady_clock::now();

  auto controlMsg = control_receiver->receive(dunedaq::iomanager::Receiver::s_block);
  auto after_control_recvd = std::chrono::steady_clock::now();
  if (verbose) {
    TLOG() << "Received control message indicating completion of reception: " << controlMsg.done_receiving;
  }

  TLOG() << "RUN TIMES: ";
  TLOG() << "Getting control connection: "
         << std::chrono::duration_cast<std::chrono::milliseconds>(after_control - start).count() << " ms";
  TLOG() << "Getting data connections: "
         << std::chrono::duration_cast<std::chrono::milliseconds>(after_senders - after_control).count() << " ms";
  TLOG() << "Starting threads: "
         << std::chrono::duration_cast<std::chrono::milliseconds>(after_send_start - after_senders).count() << " ms";
  TLOG() << "Waiting for sends to complete: "
         << std::chrono::duration_cast<std::chrono::milliseconds>(after_sends - after_send_start).count() << " ms";
  TLOG() << "Joining send threads: "
         << std::chrono::duration_cast<std::chrono::milliseconds>(after_join - after_sends).count() << " ms";
  TLOG() << "Waiting for acknowledgement from receiver: "
         << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_recvd - after_join).count() << " ms";
  TLOG() << "Send start to complete: "
         << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_recvd - after_senders).count() << " ms";
  TLOG() << "TOTAL: " << std::chrono::duration_cast<std::chrono::milliseconds>(after_control_recvd - start).count()
         << " ms";

  senders.clear();
}

int
main(int argc, char* argv[])
{
  dunedaq::logging::Logging::setup();

  bool use_connectivity_service = false;
  int port = 5000;
  std::string server = "localhost";
  size_t num_connections = 10;
  size_t num_messages = 100;
  size_t message_size_mb = 10;
  size_t num_runs = 2;
  int publish_interval = 1000;
  bool is_sender = false;
  bool is_receiver = false;
  bool verbose = false;

  namespace po = boost::program_options;
  po::options_description desc("Program to test IOManager load with many connections");
  desc.add_options()("use_connectivity_service,c",
                     po::bool_switch(&use_connectivity_service),
                     "enable the ConnectivityService in IOManager")(
    "num_connections,n", po::value<size_t>(&num_connections), "Number of connections to register and use")(
    "port,p", po::value<int>(&port), "port to connect to on configuration server")(
    "server,s", po::value<std::string>(&server), "Configuration server to connect to")(
    "num_messages,m", po::value<size_t>(&num_messages), "Number of messages to send on each connection")(
    "message_size_mb,z", po::value<size_t>(&message_size_mb), "Size of each message, in MB")(
    "num_runs,r", po::value<size_t>(&num_runs), "Number of times to clear the sender and send all messages")(
    "publish_interval,i",
    po::value<int>(&publish_interval),
    "Interval, in ms, for ConfigClient to re-publish connection info")(
    "is_sender,S", po::bool_switch(&is_sender), "Run app in sender mode (one sender and one receiver required)")(
    "is_receiver,R", po::bool_switch(&is_receiver), "Run app in receiver mode (one sender and one receiver required)")(
    "verbose,v", po::bool_switch(&verbose), "print more verbose output");

  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (std::exception& ex) {
    std::cerr << "Error parsing command line " << ex.what() << std::endl;
    std::cerr << desc << std::endl;
    return 0;
  }

  if (is_sender && is_receiver) {
    std::cerr << "Only one of --is_sender and --is_receiver should be specified, I cannot be both!" << std::endl;
    exit(2);
  }
  if (!is_sender && !is_receiver) {
    std::cerr << "One of --is_sender and --is_receiver must be specified, I don't know what to do!" << std::endl;
    exit(3);
  }

  if (use_connectivity_service) {
    TLOG() << "Setting Connectivity Service Environment variables";
    configure_connsvc(server, port);
  }

  if (is_receiver || (is_sender && !use_connectivity_service)) {
    TLOG() << "Configuring IOManager";
    configure_iomanager(num_connections, use_connectivity_service, publish_interval, verbose);
  }

  for (size_t run = 0; run < num_runs; ++run) {
    TLOG() << "Starting test run " << run;
    if (is_receiver) {
      receive(0, run, num_connections, num_messages, message_size_mb, verbose);
    } else if (is_sender) {
      send(num_connections, num_messages, message_size_mb, verbose);
    }
    TLOG() << "Test run " << run << " complete.";
  }

  TLOG() << "Cleaning up";
  dunedaq::iomanager::IOManager::get()->reset();
  TLOG() << "DONE";
};