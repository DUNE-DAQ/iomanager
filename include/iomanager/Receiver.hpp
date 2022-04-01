/**
 * @file Receiver.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_

#include "iomanager/ConnectionId.hpp"
#include "iomanager/GenericCallback.hpp"

#include "appfwk/QueueRegistry.hpp"
#include "logging/Logging.hpp"
#include "utilities/ReusableThread.hpp"
#include "ipm/Subscriber.hpp"

#include <any>
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>

namespace dunedaq {

ERS_DECLARE_ISSUE(iomanager,
                  ReceiveCallbackConflict,
                  "QueueReceiverModel for uid " << conn_uid
                                                         << " is equipped with callback! Ignoring receive call.",
                  ((std::string)conn_uid))

    ERS_DECLARE_ISSUE(iomanager, QueueNotFound, "Queue Instance not found for queue " << queue_name, ((std::string)queue_name))

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
  explicit QueueReceiverModel(ConnectionId conn_id, ConnectionRef conn_ref)
    : m_conn_id(conn_id)
    , m_conn_ref(conn_ref)
  {
    TLOG() << "QueueReceiverModel created with DT! Addr: " << this;
    // get queue ref from queueregistry based on conn_id
    // std::string sink_name = conn_id to sink_name;
    // m_source = std::make_unique<appfwk::DAQSource<Datatype>>(sink_name);
    m_queue = appfwk::QueueRegistry::get().get_queue<Datatype>(conn_id.uid);
    TLOG() << "QueueReceiverModel m_queue=" << (void*)m_queue.get();
  }

  QueueReceiverModel(QueueReceiverModel&& other)
    : m_conn_id(other.m_conn_id)
    , m_conn_ref(other.m_conn_ref)
    , m_with_callback(other.m_with_callback.load())
    , m_callback(std::move(other.m_callback))
    , m_event_loop_runner(std::move(other.m_event_loop_runner))
    , m_queue(other.m_queue)
  {}

  ~QueueReceiverModel() { remove_callback(); }

  Datatype receive(Receiver::timeout_t timeout) override
  {
    if (m_with_callback) {
      TLOG() << "QueueReceiver model is equipped with callback! Ignoring receive call.";
      throw ReceiveCallbackConflict(ERS_HERE, m_conn_id.uid);
    }
    if (m_queue == nullptr) {
      throw QueueNotFound(ERS_HERE, m_conn_id.uid);
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
      m_event_loop_runner = nullptr;
    } else if (m_event_loop_runner != nullptr) {
      TLOG() << "Event loop can't be closed!";
    }
    // remove function.
  }

  ConnectionId m_conn_id;
  ConnectionRef m_conn_ref;
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
  explicit NetworkReceiverModel(ConnectionId conn_id, ConnectionRef conn_ref)
    : m_conn_id(conn_id)
    , m_conn_ref(conn_ref)
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

  ConnectionId m_conn_id;
  ConnectionRef m_conn_ref;
  std::atomic<bool> m_with_callback{ false };
  std::function<void(Datatype&)> m_callback;
  std::unique_ptr<std::thread> m_event_loop_runner;
  std::shared_ptr<GenericCallback> m_deserializer;
  std::shared_ptr<ipm::Receiver> m_network_receiver_ptr{ nullptr };
  std::shared_ptr<ipm::Subscriber> m_network_subscriber_ptr{ nullptr };
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_
