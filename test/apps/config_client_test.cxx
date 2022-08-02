/**
 * @file config_client_test.cxx
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/network/ConfigClient.hpp"
#include "iomanager/network/ConfigClientIssues.hpp"
#include "iomanager/connection/Structs.hpp"

#include "logging/Logging.hpp"

#include "boost/program_options.hpp"
#include "nlohmann/json.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

using namespace dunedaq::iomanager;
using nlohmann::json;

int
main(int argc, char* argv[])
{

  dunedaq::logging::Logging::setup();

  std::string name("ccTest");
  std::string server("localhost");
  std::string port("5000");
  std::string file;
  std::string source = "xp2";
  namespace po = boost::program_options;
  po::options_description desc("Simple test program for ConfigClient class");
  desc.add_options()("file,f", po::value<std::string>(&file), "file to publish as our configuration")(
    "name,n", po::value<std::string>(&name), "name to publish our config under")(
    "port,p", po::value<std::string>(&port), "port to connect to on configuration server")(
    "source,r", po::value<std::string>(&source), "name of source to lookup")(
    "server,s", po::value<std::string>(&server), "Configuration server to connect to");
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (std::exception& ex) {
    std::cerr << "Error parsing command line " << ex.what() << std::endl;
    std::cerr << desc << std::endl;
    return 0;
  }

  try {
    ConfigClient dummyclient(server, port);
  } catch (EnvNotFound& ex) {
    putenv(const_cast<char*>("DUNEDAQ_PARTITION=cctest")); // NOLINT
  }
  ConfigClient client(server, port);

  std::cout << "Publishing my endpoint\n";
  Endpoint myEp;
  myEp.data_type="test";
  myEp.app_name="config_client_test";
  myEp.module_name="cc_test";
  myEp.direction=Direction::kOutput;
  client.publishEndpoint(myEp,"tcp:127.0.0.1:9999");

  std::cout << "Publishing my connection\n";
  Connection myCon;
  myCon.bind_endpoint.data_type="test";
  myCon.bind_endpoint.app_name="config_client_test2";
  myCon.bind_endpoint.module_name="cc_test";
  myCon.bind_endpoint.direction=Direction::kOutput;
  myCon.uri="tcp://192.168.1.100:1234";
  myCon.connection_type=ConnectionType::kSendRecv;
  client.publishConnection(myCon);

  std::cout << "Looking up my endpoint\n";
  auto xx=client.resolveEndpoint(myEp);

  std::cout << "Looking up my connection\n";
  std::string uri=client.resolveConnection(myCon);
  std::cout << "resolved to <" << uri << ">\n";

  std::cout << "Retracting endpoint\n";
  client.retract(myEp);

  std::cout << "Retracting connection\n";
  client.retract(myCon);

  return 0;
} // NOLINT
