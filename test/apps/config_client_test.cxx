/**
 * @file config_client_test.cxx
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/network/ConfigClient.hpp"
#include "iomanager/network/NetworkIssues.hpp"
#include "logging/Logging.hpp"

#include "boost/program_options.hpp"
#include "nlohmann/json.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <chrono>

using namespace dunedaq::iomanager;
using nlohmann::json;
using namespace std::chrono;

int
main(int argc, char* argv[])
{
  dunedaq::logging::Logging::setup();

  std::string name("ccTest");
  std::string server("localhost");
  std::string port("5000");
  std::string file;
  int connectionCount=10;
  int pause=0;
  bool useMulti=false;
  bool verbose=false;
  namespace po = boost::program_options;
  po::options_description desc("Simple test program for ConfigClient class");
  desc.add_options()(
    //"file,f", po::value<std::string>(&file), "file to publish as our configuration")(
    "name,n", po::value<std::string>(&name), "name of partition to publish our config under")(
    "count,c", po::value<int>(&connectionCount), "number of connections to publish")(
    "port,p", po::value<std::string>(&port), "port to connect to on configuration server")(
    "server,s", po::value<std::string>(&server), "Configuration server to connect to")(
    "pause,P", po::value<int>(&pause), "Pause (in seconds) between publish an lookups")(
    ",m", po::bool_switch(&useMulti), "publish using vectors of ids and uris")(
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

  std::string part="DUNEDAQ_PARTITION="+name;
  try {
    ConfigClient dummyclient(server, port);
  } catch (EnvNotFound& ex) {
    putenv(const_cast<char*>(part.c_str())); // NOLINT
  }
  ConfigClient client(server, port);

  std::vector<std::string> connections;
  std::vector<std::string> uris;
  std::ostringstream numStr;
  for (int con=0;con<connectionCount; con++) {
    numStr.str("");
    numStr << std::setfill('0') << std::setw(3) << con;
    std::string connId="DRO-"+numStr.str()+"-tp_to_trigger";
    numStr.str("");
    numStr << 1234+con;
    std::string uri="tcp://192.168.1.100:"+numStr.str();
    connections.push_back(connId);
    uris.push_back(uri);
  }

  std::cout << "Publishing my connections\n";
  auto start=system_clock::now();
  if (useMulti) {
    client.publish(connections,uris);
  }
  else {
    for (int con=0;con<connectionCount; con++) {
      client.publish(connections[con],uris[con]);
    }
  }
  auto endPublish=system_clock::now();

  if (pause>0) {
    std::cout << "  Pausing to allow initial entries to time out";
    std::cout.flush();
    for (int s=0;s<pause;s++) {
      std::this_thread::sleep_for(1s);
      std::cout << ".";
      std::cout.flush();
    }
    std::cout << std::endl;
  }

  auto startLookups=system_clock::now();
  std::cout << "Looking up connections[1]: ";
  std::cout.flush();
  std::vector<std::string> result=client.resolveConnection(connections[1]);
  if (result.size()==1) {
    std::cout << "resolved to [" << result[0] << "]\n";
  }
  else {
    std::cout << "Unexpected number of uris (" << result.size() << ")in response\n";
  }
  for (std::string dt: {"2","DRO-.*-","DRO-00[1-4]-tp_to_trigger","tp_to_trigger"}) {
    std::cout << "Looking up connections matching '" << dt << "'";
    result=client.resolveConnection(dt);
    std::cout << ".  Resolved to " << result.size() << " uris:";
    if (verbose) {
      std::cout << " [";
      for (unsigned int i=0;i<result.size();i++) {
        std::cout << result[i];
        if (i<result.size()-1) std::cout << ",";
      }
      std::cout << "]";
    }
    std::cout << std::endl;
  }
  auto endLookups=std::chrono::system_clock::now();

  std::cout << "Retracting connections\n";
  if (useMulti) {
    client.retract();
  }
  else {
    for (auto con: connections) {
      client.retract(con);
    }
  }

  auto endRetract=system_clock::now();
  double retractTime=duration_cast<microseconds>(endRetract-endLookups).count();
  double publishTime=duration_cast<microseconds>(endPublish-start).count();
  double lookupTime=duration_cast<microseconds>(endLookups-startLookups).count();
  std::cout << "Timing: publish " << publishTime/1e6
            << ", lookup " << lookupTime/1e6
            << ", retract " << retractTime/1e6
            << " seconds" << std::endl;

  return 0;
} // NOLINT
