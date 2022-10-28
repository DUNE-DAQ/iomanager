/**
 * @file Sender.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_NSENDER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_NSENDER_HPP_

#include "iomanager/Sender.hpp"
#include "iomanager/network/NetworkIssues.hpp"
#include "iomanager/network/NetworkManager.hpp"

#include "ipm/Sender.hpp"
#include "logging/Logging.hpp"
#include "serialization/Serialization.hpp"

#include <memory>
#include <string>
#include <typeinfo>
#include <utility>

namespace dunedaq::iomanager {

// NImpl
template<typename Datatype>
class NetworkSenderModel : public SenderConcept<Datatype>
{
public:
  using SenderConcept<Datatype>::send;

  explicit NetworkSenderModel(std::string const& conn_uid)
    : SenderConcept<Datatype>(conn_uid)
  {
    TLOG() << "NetworkSenderModel created with DT! Addr: " << static_cast<void*>(this);
    try {
      get_sender();
    } catch (ConnectionNotFound const& ex) {
      TLOG() << "Initial connection attempt failed: " << ex;    
   }
  }

  NetworkSenderModel(NetworkSenderModel&& other)
    : SenderConcept<Datatype>(other.m_conn.uid)
    , m_network_sender_ptr(std::move(other.m_network_sender_ptr))
    , m_topic(std::move(other.m_topic))
  {
  }

  void send(Datatype&& data, Sender::timeout_t timeout) override // NOLINT
  {
    try {
      write_network<Datatype>(data, timeout);
    } catch (ipm::SendTimeoutExpired& ex) {
      throw TimeoutExpired(ERS_HERE, this->id().uid, "send", timeout.count(), ex);
    }
  }

  bool try_send(Datatype&& data, Sender::timeout_t timeout) override // NOLINT
  {
    return try_write_network<Datatype>(data, timeout);
  }

private:
  void get_sender()
  {
    // get network resources
    try {
      m_network_sender_ptr = NetworkManager::get().get_sender(this->id());

      if (NetworkManager::get().is_pubsub_connection(this->id())) {
        TLOG() << "Setting topic to " << this->id().data_type;
        m_topic = this->id().data_type;
      }
    } catch (ers::Issue const& ex) {
      m_network_sender_ptr = nullptr;
      throw;
    }
  }

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, void>::type write_network(
    MessageType& message,
    Sender::timeout_t const& timeout)
  {
    auto start = std::chrono::steady_clock::now();
    while (m_network_sender_ptr == nullptr &&
           std::chrono::duration_cast<Sender::timeout_t>(std::chrono::steady_clock::now() - start) < timeout) {
      try {
        get_sender();
      } catch (ConnectionNotFound const& ex) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
    if (m_network_sender_ptr == nullptr) {
      ers::error(ConnectionInstanceNotFound(ERS_HERE, this->id().uid));
      throw TimeoutExpired(ERS_HERE, this->id().uid, "send", timeout.count());
    }

    auto serialized = dunedaq::serialization::serialize(message, dunedaq::serialization::kMsgPack);
    //  TLOG() << "Serialized message for network sending: " << serialized.size() << ", topic=" << m_topic << ", this="
    //  << (void*)this;
    std::lock_guard<std::mutex> lk(m_send_mutex);

    try {
      m_network_sender_ptr->send(serialized.data(), serialized.size(), timeout, m_topic);
    } catch (TimeoutExpired const& ex) {
      m_network_sender_ptr = nullptr;
      throw;
    }
  }

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, void>::type write_network(
    MessageType&,
    Sender::timeout_t const&)
  {
    throw NetworkMessageNotSerializable(ERS_HERE, typeid(MessageType).name()); // NOLINT(runtime/rtti)
  }

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, bool>::type try_write_network(
    MessageType& message,
    Sender::timeout_t const& timeout)
  {
    auto start = std::chrono::steady_clock::now();
    while (m_network_sender_ptr == nullptr &&
           std::chrono::duration_cast<Sender::timeout_t>(std::chrono::steady_clock::now() - start) < timeout) {
      try {
        get_sender();
      } catch (ConnectionNotFound const& ex) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
    if (m_network_sender_ptr == nullptr) {
      ers::error(ConnectionInstanceNotFound(ERS_HERE, this->id().uid));
      return false;
    }

    auto serialized = dunedaq::serialization::serialize(message, dunedaq::serialization::kMsgPack);
    // TLOG() << "Serialized message for network sending: " << serialized.size() << ", topic=" << m_topic <<
    // ", this=" << (void*)this;
    std::lock_guard<std::mutex> lk(m_send_mutex);

    auto res = m_network_sender_ptr->send(serialized.data(), serialized.size(), timeout, m_topic, true);
    if (!res) {
      m_network_sender_ptr = nullptr;
    }
    return res;
  }

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, bool>::type try_write_network(
    MessageType&,
    Sender::timeout_t const&)
  {
    ers::error(NetworkMessageNotSerializable(ERS_HERE, typeid(MessageType).name())); // NOLINT(runtime/rtti)
    return false;
  }

  std::shared_ptr<ipm::Sender> m_network_sender_ptr;
  std::mutex m_send_mutex;
  std::string m_topic{ "" };
};

} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
