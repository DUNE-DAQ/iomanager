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

#include "appfwk/QueueRegistry.hpp"
#include "logging/Logging.hpp"
#include "utilities/ReusableThread.hpp"

#include <any>
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>

namespace dunedaq {

ERS_DECLARE_ISSUE(iomanager,
                  ReceiveCallbackConflict,
                  "QueueReceiverModel for service_name " << service_name
                                                         << " is equipped with callback! Ignoring receive call.",
                  ((std::string)service_name))

namespace iomanager {

// Typeless
class Receiver
{
public:
  using timeout_t = std::chrono::milliseconds;
  static constexpr timeout_t s_block = timeout_t::max();
  static constexpr timeout_t s_no_block = timeout_t::zero();

  virtual ~Receiver() = default;
};

// Interface
template<typename Datatype>
class ReceiverConcept : public Receiver
{
public:
  virtual Datatype receive(Receiver::timeout_t timeout) = 0;
  virtual void add_callback(std::function<void(Datatype&)> callback) = 0;
  virtual void remove_callback() = 0;
};

// QImpl
template<typename Datatype>
class QueueReceiverModel : public ReceiverConcept<Datatype>
{
public:
  explicit QueueReceiverModel(ConnectionID conn_id)
    : m_conn_id(conn_id)
  {
    TLOG() << "QueueReceiverModel created with DT! Addr: " << this;
    // get queue ref from queueregistry based on conn_id
    // std::string sink_name = conn_id to sink_name;
    // m_source = std::make_unique<appfwk::DAQSource<Datatype>>(sink_name);
    m_queue = appfwk::QueueRegistry::get().get_queue<Datatype>(conn_id.m_service_name);
  }

  QueueReceiverModel(QueueReceiverModel&& other)
    : m_conn_id(other.m_conn_id)
    , m_with_callback(other.m_with_callback.load())
    , m_callback(std::move(other.m_callback))
    , m_event_loop_runner(std::move(other.m_event_loop_runner))
  {}

  ~QueueReceiverModel() { remove_callback(); }

  Datatype receive(Receiver::timeout_t timeout) override
  {
    if (m_with_callback) {
      TLOG() << "QueueReceiver model is equipped with callback! Ignoring receive call.";
      throw ReceiveCallbackConflict(ERS_HERE, m_conn_id.m_service_name);
    }
    if (m_queue == nullptr) {
      throw "Queue instance not found";
    }
    TLOG() << "Hand off data...";
    Datatype dt;
    m_queue->pop(dt, timeout);
    return dt;
    // if (m_queue->write(
  }

  void add_callback(std::function<void(Datatype&)> callback) override
  {
    remove_callback();
    TLOG() << "Registering callback.";
    m_callback = callback;
    m_with_callback = true;
    // start event loop (thread that calls when receive happens)
    m_event_loop_runner.reset(new std::thread([&]() {
      while (m_with_callback.load()) {
        TLOG() << "Take data from q then invoke callback...";
        Datatype dt;
        try {
          m_queue->pop(dt, std::chrono::milliseconds(500));
        } catch (appfwk::QueueTimeoutExpired&) {
        }
        m_callback(dt);
      }
    }));
  }

  void remove_callback() override
  {
    m_with_callback = false;
    if (m_event_loop_runner != nullptr && m_event_loop_runner->joinable()) {
      m_event_loop_runner->join();
    } else if (m_event_loop_runner != nullptr) {
      TLOG() << "Event loop can't be closed!";
    }
    // remove function.
  }

  ConnectionID m_conn_id;
  std::atomic<bool> m_with_callback{ false };
  std::function<void(Datatype&)> m_callback;
  std::unique_ptr<std::thread> m_event_loop_runner;
  std::shared_ptr<appfwk::Queue<Datatype>> m_queue;
  // std::unique_ptr<appfwk::DAQSource<Datatype>> m_source;
};

// NImpl
template<typename Datatype>
class NetworkReceiverModel : public ReceiverConcept<Datatype>
{
public:
  explicit NetworkReceiverModel(ConnectionID conn_id)
    : m_conn_id(conn_id)
  {
    TLOG() << "NetworkReceiverModel created with DT! Addr: " << (void*)this;
    // get network resources
  }
  ~NetworkReceiverModel() { remove_callback(); }

  NetworkReceiverModel(NetworkReceiverModel&& other)
    : m_conn_id(other.m_conn_id)
    , m_with_callback(other.m_with_callback.load())
    , m_callback(std::move(other.m_callback))
    , m_event_loop_runner(std::move(other.m_event_loop_runner))
  {}

  Datatype receive(Receiver::timeout_t /*timeout*/) override
  {
    TLOG() << "Hand off data...";
    Datatype dt;
    return dt;
    // if (m_queue->write(
  }

  void add_callback(std::function<void(Datatype&)> callback)
  {
    remove_callback();
    TLOG() << "Registering callback.";
    m_callback = callback;
    m_with_callback = true;
    // start event loop (thread that calls when receive happens)
    m_event_loop_runner.reset(new std::thread([&]() {
      while (m_with_callback.load()) {
        TLOG() << "Take data from network then invoke callback...";
        Datatype dt;
        m_callback(dt);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
    }));
  }

  void remove_callback() override
  {
    m_with_callback = false;
    if (m_event_loop_runner != nullptr && m_event_loop_runner->joinable()) {
      m_event_loop_runner->join();
    } else if (m_event_loop_runner != nullptr) {
      TLOG() << "Event loop can't be closed!";
    }
    // remove function.
  }

  ConnectionID m_conn_id;
  std::atomic<bool> m_with_callback{ false };
  std::function<void(Datatype&)> m_callback;
  std::unique_ptr<std::thread> m_event_loop_runner;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_
