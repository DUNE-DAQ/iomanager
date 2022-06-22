/**
 * @file Receiver.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_

#include "iomanager/CommonIssues.hpp"
#include "iomanager/ConnectionId.hpp"

#include "iomanager/QueueRegistry.hpp"
#include "ipm/Subscriber.hpp"
#include "logging/Logging.hpp"
#include "networkmanager/NetworkManager.hpp"
#include "serialization/Serialization.hpp"
#include "utilities/ReusableThread.hpp"

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <typeinfo>
#include <utility>

namespace dunedaq {

// Disable coverage collection LCOV_EXCL_START
ERS_DECLARE_ISSUE(iomanager,
                  ReceiveCallbackConflict,
                  "QueueReceiverModel for uid " << conn_uid << " is equipped with callback! Ignoring receive call.",
                  ((std::string)conn_uid))
// Re-enable coverage collection LCOV_EXCL_STOP

namespace iomanager {

// Typeless
class Receiver : public utilities::NamedObject
{
public:
  using timeout_t = std::chrono::milliseconds;
  static constexpr timeout_t s_block = timeout_t::max();
  static constexpr timeout_t s_no_block = timeout_t::zero();

  explicit Receiver(ConnectionId conn_id, ConnectionRef conn_ref)
    : utilities::NamedObject(conn_ref.name)
    , m_conn_id(conn_id)
    , m_conn_ref(conn_ref)
  {}
  virtual ~Receiver() = default;

  ConnectionId const conn_id() const { return m_conn_id; }
  ConnectionRef const conn_ref() const { return m_conn_ref; }

protected:
  ConnectionId m_conn_id;
  ConnectionRef m_conn_ref;
};

// Interface
template<typename Datatype>
class ReceiverConcept : public Receiver
{
public:
  explicit ReceiverConcept(ConnectionId conn_id, ConnectionRef conn_ref)
    : Receiver(conn_id, conn_ref)
  {}
  virtual Datatype receive(Receiver::timeout_t timeout) = 0;
  virtual std::optional<Datatype> try_receive(Receiver::timeout_t timeout) = 0;
  virtual void add_callback(std::function<void(Datatype&)> callback) = 0;
  virtual void remove_callback() = 0;
};

// QImpl
template<typename Datatype>
class QueueReceiverModel : public ReceiverConcept<Datatype>
{
public:
  explicit QueueReceiverModel(ConnectionId conn_id, ConnectionRef conn_ref)
    : ReceiverConcept<Datatype>(conn_id, conn_ref)
  {
    TLOG() << "QueueReceiverModel created with DT! Addr: " << this;
    // get queue ref from queueregistry based on conn_id
    // std::string sink_name = conn_id to sink_name;
    // m_source = std::make_unique<appfwk::DAQSource<Datatype>>(sink_name);
    m_queue = QueueRegistry::get().get_queue<Datatype>(conn_id.uid);
    TLOG() << "QueueReceiverModel m_queue=" << static_cast<void*>(m_queue.get());
  }

  QueueReceiverModel(QueueReceiverModel&& other)
    : ReceiverConcept<Datatype>(other.m_conn_id, other.m_conn_ref)
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
      throw ReceiveCallbackConflict(ERS_HERE, this->conn_id().uid);
    }
    if (m_queue == nullptr) {
      throw ConnectionInstanceNotFound(ERS_HERE, this->conn_id().uid);
    }
    // TLOG() << "Hand off data...";
    Datatype dt;
    try {
      m_queue->pop(dt, timeout);
    } catch (QueueTimeoutExpired& ex) {
      throw TimeoutExpired(ERS_HERE, this->conn_id().uid, "pop", timeout.count(), ex);
    }
    return dt;
    // if (m_queue->write(
  }

  std::optional<Datatype> try_receive(Receiver::timeout_t timeout) override
  {
    if (m_with_callback) {
      TLOG() << "QueueReceiver model is equipped with callback! Ignoring receive call.";
      ers::error(ReceiveCallbackConflict(ERS_HERE, this->conn_id().uid));
      return std::nullopt;
    }
    if (m_queue == nullptr) {
      ers::error(ConnectionInstanceNotFound(ERS_HERE, this->conn_id().uid));
      return std::nullopt;
    }
    // TLOG() << "Hand off data...";
    Datatype dt;
    auto ret = m_queue->try_pop(dt, timeout);
    if (ret) {
      return std::make_optional(std::move(dt));
    }
    return std::nullopt;
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
      Datatype dt;
      bool ret = true;
      while (m_with_callback.load() || ret) {
        // TLOG() << "Take data from q then invoke callback...";
        ret = m_queue->try_pop(dt, std::chrono::milliseconds(1));
        if (ret) {
          m_callback(dt);
        }
      }
    }));
  }

  void remove_callback() override
  {
    m_with_callback = false;
    if (m_event_loop_runner != nullptr && m_event_loop_runner->joinable()) {
      m_event_loop_runner->join();
      m_event_loop_runner.reset(nullptr);
    } else if (m_event_loop_runner != nullptr) {
      TLOG() << "Event loop can't be closed!";
    }
    // remove function.
  }

private:
  std::atomic<bool> m_with_callback{ false };
  std::function<void(Datatype&)> m_callback;
  std::unique_ptr<std::thread> m_event_loop_runner;
  std::shared_ptr<Queue<Datatype>> m_queue;
};

// NImpl
template<typename Datatype>
class NetworkReceiverModel : public ReceiverConcept<Datatype>
{
public:
  explicit NetworkReceiverModel(ConnectionId conn_id, ConnectionRef conn_ref, bool ref_to_topic = false)
    : ReceiverConcept<Datatype>(conn_id, conn_ref)
  {
    TLOG() << "NetworkReceiverModel created with DT! ID: " << (ref_to_topic ? conn_ref.uid : conn_id.uid)
           << " Addr: " << static_cast<void*>(this);
    // get network resources
    if (conn_id.service_type == ServiceType::kNetReceiver) {

      try {
        m_network_receiver_ptr = networkmanager::NetworkManager::get().get_receiver(conn_id.uid);
      } catch (networkmanager::ConnectionNotFound& ex) {
        throw ConnectionInstanceNotFound(ERS_HERE, conn_id.uid, ex);
      }
    } else {
      try {
        if (ref_to_topic) {
          m_network_subscriber_ptr = networkmanager::NetworkManager::get().get_subscriber(conn_ref.uid);
        } else {
          m_network_subscriber_ptr =
            std::dynamic_pointer_cast<ipm::Subscriber>(networkmanager::NetworkManager::get().get_receiver(conn_id.uid));
        }
      } catch (networkmanager::ConnectionNotFound& ex) {
        throw ConnectionInstanceNotFound(ERS_HERE, conn_ref.uid, ex);
      }
    }
  }
  ~NetworkReceiverModel() { remove_callback(); }

  NetworkReceiverModel(NetworkReceiverModel&& other)
    : ReceiverConcept<Datatype>(other.m_conn_id, other.m_conn_ref)
    , m_with_callback(other.m_with_callback.load())
    , m_callback(std::move(other.m_callback))
    , m_event_loop_runner(std::move(other.m_event_loop_runner))
    , m_network_receiver_ptr(other.m_network_receiver_ptr)
    , m_network_subscriber_ptr(other.m_network_subscriber_ptr)
  {}

  Datatype receive(Receiver::timeout_t timeout) override
  {
    try {
      return read_network<Datatype>(timeout);
    } catch (ipm::ReceiveTimeoutExpired& ex) {
      throw TimeoutExpired(ERS_HERE, this->conn_ref().uid, "receive", timeout.count(), ex);
    }
  }

  std::optional<Datatype> try_receive(Receiver::timeout_t timeout) override
  {
    return try_read_network<Datatype>(timeout);
  }
  void add_callback(std::function<void(Datatype&)> callback) { add_callback_impl<Datatype>(callback); }

  void remove_callback() override
  {
    std::lock_guard<std::mutex> lk(m_callback_mutex);
    m_with_callback = false;
    if (m_event_loop_runner != nullptr && m_event_loop_runner->joinable()) {
      m_event_loop_runner->join();
      m_event_loop_runner.reset(nullptr);
    } else if (m_event_loop_runner != nullptr) {
      TLOG() << "Event loop can't be closed!";
    }
    // remove function.
  }

private:
  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, MessageType>::type read_network(
    Receiver::timeout_t const& timeout)
  {
    std::lock_guard<std::mutex> lk(m_receive_mutex);

    if (m_network_subscriber_ptr != nullptr) {
      auto response = m_network_subscriber_ptr->receive(timeout);
      if (response.data.size() > 0) {
        return dunedaq::serialization::deserialize<MessageType>(response.data);
      }
    }
    if (m_network_receiver_ptr != nullptr) {
      auto response = m_network_receiver_ptr->receive(timeout);
      if (response.data.size() > 0) {
        return dunedaq::serialization::deserialize<MessageType>(response.data);
      }
    }

    throw TimeoutExpired(ERS_HERE, this->conn_id().uid, "network receive", timeout.count());
    return MessageType();
  }

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, MessageType>::type read_network(
    Receiver::timeout_t const&)
  {
    throw NetworkMessageNotSerializable(ERS_HERE, typeid(MessageType).name());
    return MessageType();
  }

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, std::optional<MessageType>>::type
      try_read_network(Receiver::timeout_t const& timeout)
  {
    ipm::Receiver::Response res;
    std::lock_guard<std::mutex> lk(m_receive_mutex);

    if (m_network_subscriber_ptr != nullptr) {
      res = m_network_subscriber_ptr->receive(timeout, ipm::Receiver::s_any_size, true);
    }
    if (m_network_receiver_ptr != nullptr) {
      res = m_network_receiver_ptr->receive(timeout, ipm::Receiver::s_any_size, true);
    }

    if (res.data.size() > 0) {
      return std::make_optional<MessageType>(dunedaq::serialization::deserialize<MessageType>(res.data));
    }

    return std::nullopt;
  }

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value,
                          std::optional<MessageType>>::type
      try_read_network(Receiver::timeout_t const&)
  {
    ers::error(NetworkMessageNotSerializable(ERS_HERE, typeid(MessageType).name()));
    return std::nullopt;
  }

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, void>::type add_callback_impl(
    std::function<void(MessageType&)> callback)
  {
    remove_callback();
    {
      std::lock_guard<std::mutex> lk(m_callback_mutex);
    }
    TLOG() << "Registering callback.";
    m_callback = callback;
    m_with_callback = true;
    // start event loop (thread that calls when receive happens)
    m_event_loop_runner.reset(new std::thread([&]() {
        std::optional<Datatype> message;
      while (m_with_callback.load() || message ) {
        try {
          message = try_read_network<Datatype>(std::chrono::milliseconds(1));
          if (message) {
              m_callback(*message);
          }
        } catch (const ers::Issue&) {
          ;
        }
      }
    }));
  }

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, void>::type add_callback_impl(
    std::function<void(MessageType&)>)
  {
    throw NetworkMessageNotSerializable(ERS_HERE, typeid(MessageType).name());
  }

  std::atomic<bool> m_with_callback{ false };
  std::function<void(Datatype&)> m_callback;
  std::unique_ptr<std::thread> m_event_loop_runner;
  std::shared_ptr<ipm::Receiver> m_network_receiver_ptr{ nullptr };
  std::shared_ptr<ipm::Subscriber> m_network_subscriber_ptr{ nullptr };
  std::mutex m_callback_mutex;
  std::mutex m_receive_mutex;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_
