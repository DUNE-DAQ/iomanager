/**
 * @file config_client_test.cxx
 *
 * Tests the connection to the connectivity service, and whether publishes
 * and lookups work as expected.
 *
 * Run "config_client_test --help" to see options
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/network/ConfigClient.hpp"
#include "iomanager/network/NetworkIssues.hpp"
#include "logging/Logging.hpp"

#include "confmodel/NetworkConnection.hpp"

#include "boost/program_options.hpp"
#include "nlohmann/json.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace dunedaq::iomanager;
using nlohmann::json;
using namespace std::chrono;

int
main(int argc, char* argv[])
{

  std::string name("ccTest");
  std::string server("localhost");
  std::string port("5000");
  std::string file;
  int connection_count = 10;
  int pause = 0;
  bool use_multi = false;
  bool verbose = false;
  namespace po = boost::program_options;
  po::options_description desc("Simple test program for ConfigClient class");
  desc.add_options()(
    //"file,f", po::value<std::string>(&file), "file to publish as our configuration")(
    "name,n",
    po::value<std::string>(&name),
    "name of session to publish our config under")(
    "count,c", po::value<int>(&connection_count), "number of connections to publish")(
    "port,p", po::value<std::string>(&port), "port to connect to on configuration server")(
    "server,s", po::value<std::string>(&server), "Configuration server to connect to")(
    "pause,P", po::value<int>(&pause), "Pause (in seconds) between publish an lookups")(
    ",m", po::bool_switch(&use_multi), "publish using vectors of ids and uris")(
    "verbose,v", po::bool_switch(&verbose), "print more verbose output");

  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (std::exception& ex) {
    std::cerr << "Error parsing command line " << ex.what() << std::endl; // NOLINT
    std::cerr << desc << std::endl;                                       // NOLINT
    return 0;
  }

  dunedaq::logging::Logging::setup(name, "config_client_test");

  ConfigClient client(server, port, name, 1000ms);

  std::vector<ConnectionRegistration> connections;
  std::ostringstream num_str;
  for (int con = 0; con < connection_count; con++) {
    num_str.str("");
    num_str << std::setfill('0') << std::setw(3) << con;
    std::string conn_id = "DRO-" + num_str.str() + "-tp_to_trigger";
    num_str.str("");
    num_str << 1234 + con;
    std::string uri = "tcp://192.168.1.100:" + num_str.str();
    ConnectionRegistration conn_reg;
    conn_reg.uid = conn_id;
    conn_reg.data_type = "TPSet";
    conn_reg.uri = uri;
    conn_reg.connection_type = dunedaq::iomanager::ConnectionType::kSendRecv;
    connections.push_back(conn_reg);
  }

  std::cout << "Publishing my connections\n"; // NOLINT
  auto start = system_clock::now();
  if (use_multi) {
    client.publish(connections);
  } else {
    for (int con = 0; con < connection_count; con++) {
      client.publish(connections[con]);
    }
  }
  auto end_publish = system_clock::now();

  if (pause > 0) {
    std::cout << "  Pausing to allow initial entries to time out"; // NOLINT
    std::cout.flush();                                             // NOLINT
    for (int s = 0; s < pause; s++) {
      std::this_thread::sleep_for(1s);
      std::cout << ".";  // NOLINT
      std::cout.flush(); // NOLINT
    }
    std::cout << std::endl; // NOLINT
  }

  auto start_lookups = system_clock::now();
  std::cout << "Looking up connections[1]: "; // NOLINT
  std::cout.flush();                          // NOLINT
  ConnectionRequest req;
  req.data_type = connections[0].data_type;
  req.uid_regex = connections[0].uid;
  auto result = client.resolve_connection(req);
  if (result.connections.size() == 1) {
    std::cout << "resolved to [" << result.connections[0].uid << "]\n"; // NOLINT
  } else {
    std::cout << "Unexpected number of uris (" << result.connections.size() << ")in response\n"; // NOLINT
  }
  for (std::string dt : { "2", "DRO-.*-", "DRO-00[1-4]-tp_to_trigger", "tp_to_trigger" }) {
    std::cout << "Looking up connections matching '" << dt << "'"; // NOLINT
    req.uid_regex = dt;
    result = client.resolve_connection(req);
    std::cout << ".  Resolved to " << result.connections.size() << " uris:"; // NOLINT
    if (verbose) {
      std::cout << " ["; // NOLINT
      for (unsigned int i = 0; i < result.connections.size(); i++) {
        std::cout << result.connections[i].uri; // NOLINT
        if (i < result.connections.size() - 1)
          std::cout << ","; // NOLINT
      }
      std::cout << "]"; // NOLINT
    }
    std::cout << std::endl; // NOLINT
  }
  auto end_lookups = std::chrono::system_clock::now();

  std::cout << "Retracting connections\n"; // NOLINT
  if (use_multi) {
    client.retract();
  } else {
    for (auto const& con : connections) {
      ConnectionId id;
      id.uid = con.uid;
      id.data_type = con.data_type;
      client.retract(id);
    }
  }

  auto end_retract = system_clock::now();
  double retract_time = static_cast<double>(duration_cast<microseconds>(end_retract - end_lookups).count());
  double publish_time = static_cast<double>(duration_cast<microseconds>(end_publish - start).count());
  double lookup_time = static_cast<double>(duration_cast<microseconds>(end_lookups - start_lookups).count());
  std::cout << "Timing: publish " << publish_time / 1e6 << ", lookup " << lookup_time / 1e6 << ", retract " // NOLINT
            << retract_time / 1e6 << " seconds" << std::endl;

  return 0;
} // NOLINT
