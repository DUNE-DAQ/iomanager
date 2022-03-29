/**
 * @file test_io_manager_app.cxx Test application for
 * demonstrating IOManager skeleton.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#include "logging/Logging.hpp"

#include "iomanager/ConnectionID.hpp"
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
  dunedaq::iomanager::IOManager iom;

  std::cout << "Test int sender.\n";
  // Int sender
  dunedaq::iomanager::ConnectionID cid;
  cid.m_service_type = "foo";
  cid.m_service_name = "bar";
  cid.m_topic = "";

  int msg = 5;
  auto isender = iom.get_sender<int>(cid);
  std::cout << "Type: " << typeid(isender).name() << '\n';
  isender->send(msg);
  isender->send(msg);
  std::cout << "\n\n";

  std::cout << "Test one line sender.\n";
  // One line send
  iom.get_sender<int>(cid)->send(msg);
  std::cout << "\n\n\n";

  std::cout << "Test string sender.\n";
  // String sender
  dunedaq::iomanager::ConnectionID cid2;
  cid2.m_service_type = "bar";
  cid2.m_service_name = "foo";
  cid2.m_topic = "";

  auto ssender = iom.get_sender<std::string>(cid2);
  std::cout << "Type: " << typeid(ssender).name() << '\n';
  std::string asd("asd");
  ssender->send(asd);
  std::cout << "\n\n";

  std::cout << "Test string receiver.\n";
  // String receiver
  dunedaq::iomanager::ConnectionID cid3;
  cid3.m_service_type = "asd";
  cid3.m_service_name = "dsa";
  cid3.m_topic = "";

  auto receiver = iom.get_receiver<std::string>(cid3);
  std::cout << "Type: " << typeid(receiver).name() << '\n';
  std::string got = receiver->receive();
  std::cout << "\n\n";

  std::cout << "Test callback string receiver.\n";
  // Callback receiver
  dunedaq::iomanager::ConnectionID cid4;
  cid4.m_service_type = "xyz";
  cid4.m_service_name = "zyx";
  cid4.m_topic = "";

  // CB function and run-marker
  std::atomic<bool> run_marker{ true };
  std::function<void(std::string)> str_receiver_cb = [&](std::string data) {
    std::cout << "Str receiver callback called with data: " << data << '\n';
  };

  auto cbrec = iom.get_receiver<std::string>(cid4);
  std::cout << "Type: " << typeid(cbrec).name() << '\n';
  cbrec->add_callback(str_receiver_cb, run_marker);
  std::cout << "Try to call receive, which should fail with callbacks registered!\n";
  got = cbrec->receive();

  // Exercise internal event loop
  std::cout << "Wait a bit in main to see event loop polling...\n";
  std::this_thread::sleep_for(std::chrono::seconds(5));
  std::cout << "Flip killswitch/run_marker... Then unregister callback for event loop stop.\n";
  run_marker.store(false);
  // iom.unregister_callback<std::string>(cid4); or:
  cbrec->remove_callback();
  std::cout << "\n\n";

  // Exit
  TLOG() << "Exiting.";
  return 0;
}
