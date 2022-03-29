/**
 * @file Receiver.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_

#include "iomanager/ConnectionID.hpp"
#include "iomanager/GenericCallback.hpp"
#include "utilities/ReusableThread.hpp"

#include <any>
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>

namespace dunedaq {
namespace iomanager {

// Typeless
class Receiver
{
public:
  virtual ~Receiver() = default;
};

// Interface
template<typename Datatype>
class ReceiverConcept : public Receiver
{
public:
  virtual Datatype receive() = 0;
  virtual void add_callback(std::function<void(Datatype)> callback, std::atomic<bool>& run_marker) = 0;
  virtual void remove_callback() = 0;
};

// QImpl
template<typename Datatype>
class QueueReceiverModel : public ReceiverConcept<Datatype>
{
public:
  explicit QueueReceiverModel(ConnectionID conn_id)
    : m_conn_id(conn_id)
    , m_with_callback{ false }
  {
    TLOG() << "QueueReceiverModel created with DT! Addr: " << this;
    // get queue ref from queueregistry based on conn_id
    // std::string sink_name = conn_id to sink_name;
    // m_source = std::make_unique<appfwk::DAQSource<Datatype>>(sink_name);
  }

  Datatype receive() override
  {
    if (m_with_callback) {
      TLOG() << "QueueReceiver model is equipped with callback! Ignoring receive call.";
      Datatype dt;
      return dt;
    }
    TLOG() << "Hand off data...";
    Datatype dt;
    return dt;
    // if (m_queue->write(
  }

  void add_callback(std::function<void(Datatype)> callback, std::atomic<bool>& run_marker) override
  {
    TLOG() << "Registering callback.";
    m_callback = callback;
    m_with_callback = true;
    // start event loop (thread that calls when receive happens)
    m_event_loop_runner = std::thread([&]() {
      while (run_marker.load()) {
        TLOG() << "Take data from q then invoke callback...";
        Datatype dt;
        m_callback(dt);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
    });
  }

  void remove_callback() override
  {
    if (m_event_loop_runner.joinable()) {
      m_event_loop_runner.join();
    } else {
      TLOG() << "Event loop can't be closed!";
    }
    // remove function.
  }

  ConnectionID m_conn_id;
  bool m_with_callback;
  std::function<void(Datatype)> m_callback;
  std::thread m_event_loop_runner;
  // std::unique_ptr<appfwk::DAQSource<Datatype>> m_source;
};

// NImpl
template<typename Datatype>
class NetworkReceiverModel : public ReceiverConcept<Datatype>
{
public:
  explicit NetworkReceiverModel(ConnectionID conn_id)
    : ReceiverConcept<Datatype>(conn_id)
    , m_conn_id(conn_id)
  {
    TLOG() << "NetworkReceiverModel created with DT! Addr: " << (void*)this;
    // get network resources
  }

  Datatype receive() override
  {
    TLOG() << "Hand off data...";
    Datatype dt;
    return dt;
    // if (m_queue->write(
  }

  void add_callback(std::function<void(Datatype)> callback, std::atomic<bool>& run_marker)
  {
    TLOG() << "Registering callback.";
    m_callback = callback;
    m_with_callback = true;
    // start event loop (thread that calls when receive happens)
    m_event_loop_runner = std::thread([&]() {
      while (run_marker.load()) {
        TLOG() << "Take data from network then invoke callback...";
        Datatype dt;
        m_callback(dt);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
    });
  }

  void remove_callback() override
  {
    if (m_event_loop_runner.joinable()) {
      m_event_loop_runner.join();
    } else {
      TLOG() << "Event loop can't be closed!";
    }
    // remove function.
  }

  ConnectionID m_conn_id;
  bool m_with_callback;
  std::function<void(Datatype)> m_callback;
  std::thread m_event_loop_runner;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_
