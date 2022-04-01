/**
 * @file test_io_manager_app.cxx Test application for
 * demonstrating IOManager skeleton.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#include "logging/Logging.hpp"

#include "iomanager/ConnectionId.hpp"
#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>

int
main(int /*argc*/, char** /*argv[]*/)
{
  std::map<std::string, dunedaq::appfwk::QueueConfig> config_map;
  dunedaq::appfwk::QueueConfig qspec;
  qspec.kind = dunedaq::appfwk::QueueConfig::queue_kind::kStdDeQueue;
  qspec.capacity = 10;
  config_map["bar"] = qspec;
  config_map["foo"] = qspec;
  config_map["dsa"] = qspec;
  config_map["zyx"] = qspec;
  dunedaq::appfwk::QueueRegistry::get().configure(config_map);

  dunedaq::iomanager::IOManager iom;
  dunedaq::iomanager::ConnectionIds_t connections;
  dunedaq::iomanager::ConnectionId cid;
  cid.service_type = dunedaq::iomanager::ServiceType::kQueue;
  cid.uid = "bar";
  cid.data_type = "int";
  connections.push_back(cid);
  cid.uid = "foo";
  connections.push_back(cid);
  cid.uid = "dsa";
  connections.push_back(cid);
  cid.uid = "zyx";
  cid.data_type = "int";
  connections.push_back(cid);
  iom.configure(connections);

  std::cout << "Test int sender.\n";
  // Int sender
  dunedaq::iomanager::ConnectionRef cref;
  cref.uid = "bar";
  cref.topics = {};

  int msg = 5;
  std::chrono::milliseconds timeout(100);
  auto isender = iom.get_sender<int>(cref);
  std::cout << "Type: " << typeid(isender).name() << '\n';
  isender->send(msg, timeout);
  isender->send(msg, timeout);
  std::cout << "\n\n";

  std::cout << "Test one line sender.\n";
  // One line send
  iom.get_sender<int>(cref)->send(msg, timeout);
  std::cout << "\n\n\n";

  std::cout << "Test string sender.\n";
  // String sender
  dunedaq::iomanager::ConnectionRef cref2;
  cref2.uid = "foo";
  cref2.topics = {};

  auto ssender = iom.get_sender<std::string>(cref2);
  std::cout << "Type: " << typeid(ssender).name() << '\n';
  std::string asd("asd");
  ssender->send(asd, timeout);
  std::cout << "\n\n";

  std::cout << "Test string receiver.\n";
  // String receiver
  dunedaq::iomanager::ConnectionRef cref3;
  cref3.uid = "dsa";
  cref3.topics = {};

  auto receiver = iom.get_receiver<std::string>(cref3);
  std::cout << "Type: " << typeid(receiver).name() << '\n';
  std::string got;
  try {
    got = receiver->receive(timeout);
  } catch (dunedaq::appfwk::QueueTimeoutExpired&) {
    // This is expected
  }
  std::cout << "\n\n";

  std::cout << "Test callback string receiver.\n";
  // Callback receiver
  dunedaq::iomanager::ConnectionRef cref4;
  cref4.uid = "zyx";
  cref4.topics = {};

  // CB function
  std::function<void(std::string)> str_receiver_cb = [&](std::string data) {
    std::cout << "Str receiver callback called with data: " << data << '\n';
  };

  auto cbrec = iom.get_receiver<std::string>(cref4);
  std::cout << "Type: " << typeid(cbrec).name() << '\n';
  cbrec->add_callback(str_receiver_cb);
  std::cout << "Try to call receive, which should fail with callbacks registered!\n";
  try {
    got = cbrec->receive(timeout);
  } catch (dunedaq::iomanager::ReceiveCallbackConflict&) {
    // This is expected
  }

  // Exercise internal event loop
  std::cout << "Wait a bit in main to see event loop polling...\n";
  std::this_thread::sleep_for(std::chrono::seconds(5));
  std::cout << "Unregister callback for event loop stop.\n";
  cbrec->remove_callback();
  std::cout << "\n\n";

  // Exit
  TLOG() << "Exiting.";
  return 0;
}
