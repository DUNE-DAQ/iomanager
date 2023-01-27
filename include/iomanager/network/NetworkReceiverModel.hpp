/**
 * @file Receiver.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_NRECEIVER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_NRECEIVER_HPP_

#include "iomanager/Receiver.hpp"
#include "iomanager/network/NetworkIssues.hpp"
#include "iomanager/network/NetworkManager.hpp"

#include "ipm/Subscriber.hpp"
#include "logging/Logging.hpp"
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

namespace iomanager {

// NImpl
template<typename Datatype>
class NetworkReceiverModel : public ReceiverConcept<Datatype>
{
public:
  explicit NetworkReceiverModel(ConnectionId const& conn_id)
    : ReceiverConcept<Datatype>(conn_id)
  {
    TLOG() << "NetworkReceiverModel created with DT! ID: " << conn_id.uid << " Addr: " << static_cast<void*>(this);
    try {
      get_receiver(std::chrono::milliseconds(1000));
    } catch (ConnectionNotFound const& ex) {
      TLOG() << "Initial connection attempt failed: " << ex;
    }
  }
  ~NetworkReceiverModel() { remove_callback(); }

  NetworkReceiverModel(NetworkReceiverModel&& other)
    : ReceiverConcept<Datatype>(other.m_conn.uid)
    , m_with_callback(other.m_with_callback.load())
    , m_callback(std::move(other.m_callback))
    , m_event_loop_runner(std::move(other.m_event_loop_runner))
    , m_network_receiver_ptr(std::move(other.m_network_receiver_ptr))
  {
  }

  Datatype receive(Receiver::timeout_t timeout) override
  {
    try {
      return read_network<Datatype>(timeout);
    } catch (ipm::ReceiveTimeoutExpired& ex) {
      throw TimeoutExpired(ERS_HERE, this->id().uid, "receive", timeout.count(), ex);
    }
  }

  std::optional<Datatype> try_receive(Receiver::timeout_t timeout) override
  {
    return try_read_network<Datatype>(timeout);
  }
  void add_callback(std::function<void(Datatype&)> callback) override { add_callback_impl<Datatype>(callback); }

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

  void subscribe(std::string topic) override
  {
    if (NetworkManager::get().is_pubsub_connection(this->m_conn)) {
        std::dynamic_pointer_cast<ipm::Subscriber>(m_network_receiver_ptr)->subscribe(topic);
    }
  }
  void unsubscribe(std::string topic) override
  {
    if (NetworkManager::get().is_pubsub_connection(this->m_conn)) {
        std::dynamic_pointer_cast<ipm::Subscriber>(m_network_receiver_ptr)->unsubscribe(topic);
    }
  }

private:
  void get_receiver(Receiver::timeout_t timeout)
  {
    // get network resources
    auto start = std::chrono::steady_clock::now();
    while (m_network_receiver_ptr == nullptr &&
           std::chrono::duration_cast<Receiver::timeout_t>(std::chrono::steady_clock::now() - start) < timeout) {

      try {
        m_network_receiver_ptr = NetworkManager::get().get_receiver(this->id());
      } catch (ConnectionNotFound const& ex) {
        m_network_receiver_ptr = nullptr;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  }

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, MessageType>::type read_network(
    Receiver::timeout_t const& timeout)
  {
    std::lock_guard<std::mutex> lk(m_receive_mutex);
    get_receiver(timeout);

    if (m_network_receiver_ptr == nullptr) {
      throw ConnectionInstanceNotFound(ERS_HERE, this->id().uid);
    }

    auto response = m_network_receiver_ptr->receive(timeout);
    if (response.data.size() > 0) {
      return dunedaq::serialization::deserialize<MessageType>(response.data);
    }

    throw TimeoutExpired(ERS_HERE, this->id().uid, "network receive", timeout.count());
    return MessageType();
  }

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, MessageType>::type read_network(
    Receiver::timeout_t const&)
  {
    throw NetworkMessageNotSerializable(ERS_HERE, typeid(MessageType).name()); // NOLINT(runtime/rtti)
    return MessageType();
  }

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, std::optional<MessageType>>::type
  try_read_network(Receiver::timeout_t const& timeout)
  {
    std::lock_guard<std::mutex> lk(m_receive_mutex);
    get_receiver(timeout);
    if (m_network_receiver_ptr == nullptr) {
      ers::error(ConnectionInstanceNotFound(ERS_HERE, this->id().uid));
      return std::nullopt;
    }

    ipm::Receiver::Response res;
    res = m_network_receiver_ptr->receive(timeout, ipm::Receiver::s_any_size, true);

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
    ers::error(NetworkMessageNotSerializable(ERS_HERE, typeid(MessageType).name())); // NOLINT(runtime/rtti)
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
    m_event_loop_runner = std::make_unique<std::thread>([&]() {
      std::optional<Datatype> message;
      while (m_with_callback.load() || message) {
        try {
          message = try_read_network<Datatype>(std::chrono::milliseconds(1));
          if (message) {
            m_callback(*message);
          }
        } catch (const ers::Issue&) {
          ;
        }
      }
    });
  }

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, void>::type add_callback_impl(
    std::function<void(MessageType&)>)
  {
    throw NetworkMessageNotSerializable(ERS_HERE, typeid(MessageType).name()); // NOLINT(runtime/rtti)
  }

  std::atomic<bool> m_with_callback{ false };
  std::function<void(Datatype&)> m_callback;
  std::unique_ptr<std::thread> m_event_loop_runner;
  std::shared_ptr<ipm::Receiver> m_network_receiver_ptr{ nullptr };
  std::mutex m_callback_mutex;
  std::mutex m_receive_mutex;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_
