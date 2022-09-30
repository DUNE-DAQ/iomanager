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

void printResponse(ConnectionResponse response) {
  std::cout << "Endpoint resolved to uris {";
  auto uiter=response.uris.begin();
  while (uiter != response.uris.end()) {
    std::cout << *uiter;
    uiter++;
    if (uiter!=response.uris.end()) std::cout << ",";
  }
  std::cout << "}, connection type: " << str(response.connection_type) << std::endl;
}

int
main(int argc, char* argv[])
{

  dunedaq::logging::Logging::setup();

  std::string name("ccTest");
  std::string server("localhost");
  std::string port("5000");
  std::string file;
  int connectionCount=10;
  namespace po = boost::program_options;
  po::options_description desc("Simple test program for ConfigClient class");
  desc.add_options()(
    //"file,f", po::value<std::string>(&file), "file to publish as our configuration")(
    "name,n", po::value<std::string>(&name), "name to publish our config under")(
    "count,c", po::value<int>(&connectionCount), "number of connections to publish")(
    "port,p", po::value<std::string>(&port), "port to connect to on configuration server")(
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
  myEp.app_name=name;
  myEp.module_name="cc_test";
  myEp.direction=Direction::kOutput;
  client.publishEndpoint(myEp,"tcp:127.0.0.1:9999");

  std::cout << "Publishing my connections\n";
  std::vector<Connection> connections;
  std::ostringstream numStr;
  for (int con=0;con<connectionCount; con++) {
    numStr.str("");
    numStr << con;
    Connection myCon;
    myCon.bind_endpoint.data_type=con%2? "try":"test";
    myCon.bind_endpoint.app_name=name+numStr.str();
    myCon.bind_endpoint.module_name="cc_test";
    myCon.bind_endpoint.source_id={Subsystem::kDetectorReadout,con};
    myCon.bind_endpoint.direction=Direction::kOutput;
    numStr.str("");
    numStr << 1234+con;
    myCon.uri="tcp://192.168.1.100:"+numStr.str();
    myCon.connection_type=ConnectionType::kSendRecv;
    client.publishConnection(myCon);
    connections.push_back(myCon);
  }
  std::cout << "Looking up connection[1]\n";
  std::string uri=client.resolveConnection(connections[1]);
  std::cout << "resolved to <" << uri << ">\n";

  ConnectionRequest myQuery;
  ConnectionResponse response;
  myQuery.app_name="*";
  myQuery.module_name="*";
  for (std::string dt: {"test","try"}) {
    myQuery.data_type=dt;
    std::cout << "Looking up endPoints with data type " << myQuery.data_type << std::endl;
    response=client.resolveEndpoint(myQuery);
    printResponse(response);
  }

  for (int con=0;con<2; con++) {
    numStr.str("");
    numStr << con;
    myQuery.app_name=name+numStr.str();
    std::cout << "Looking up endPoints with data type " << myQuery.data_type
              << " and app_name " << myQuery.app_name <<std::endl;
    //myQuery.module_name="cc_test";
    try {
      response=client.resolveEndpoint(myQuery);
      printResponse(response);
    }
    catch (FailedLookup& exc) {
      std::cout << "Not found" << std::endl;
    }
  }

  std::cout << "Retracting endpoint\n";
  client.retract(myEp);

  std::cout << "Retracting connections\n";
  for (Connection myCon: connections) {
    client.retract(myCon);
  }
  return 0;
} // NOLINT
