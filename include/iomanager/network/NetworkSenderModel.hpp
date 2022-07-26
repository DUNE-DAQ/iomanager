/**
 * @file Sender.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_NSENDER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_NSENDER_HPP_

#include "iomanager/CommonIssues.hpp"
#include "iomanager/Sender.hpp"
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

  explicit NetworkSenderModel(Endpoint const& this_endpoint, ConnectionType const& type_hint)
    : SenderConcept<Datatype>(this_endpoint)
  {
    TLOG() << "NetworkSenderModel created with DT! Addr: " << static_cast<void*>(this);
    // get network resources
    m_network_sender_ptr = NetworkManager::get().get_sender(this_endpoint, type_hint);

    if (type_hint == ConnectionType::kPubSub) {
      m_topic = this_endpoint.data_type;
    }
  }

  NetworkSenderModel(NetworkSenderModel&& other)
    : SenderConcept<Datatype>(other.m_conn_id, other.m_conn_ref)
    , m_network_sender_ptr(std::move(other.m_network_sender_ptr))
  {}

  void send(Datatype&& data, Sender::timeout_t timeout) override // NOLINT
  {
    try {
      write_network<Datatype>(data, timeout);
    } catch (ipm::SendTimeoutExpired& ex) {
      throw TimeoutExpired(ERS_HERE, to_string(this->endpoint()), "send", timeout.count(), ex);
    }
  }

  bool try_send(Datatype&& data, Sender::timeout_t timeout) override // NOLINT
  {
    return try_write_network<Datatype>(data, timeout);
  }

private:
  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, void>::type
  write_network(MessageType& message, Sender::timeout_t const& timeout)
  {
    if (m_network_sender_ptr == nullptr)
      throw ConnectionInstanceNotFound(ERS_HERE, to_string(this->endpoint()));

    auto serialized = dunedaq::serialization::serialize(message, dunedaq::serialization::kMsgPack);
    // TLOG() << "Serialized message for network sending: " << serialized.size() << ", this=" << (void*)this;
    std::lock_guard<std::mutex> lk(m_send_mutex);

    m_network_sender_ptr->send(serialized.data(), serialized.size(), timeout, m_topic);
  }

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, void>::type
  write_network(MessageType&, Sender::timeout_t const&, std::string const&)
  {
    throw NetworkMessageNotSerializable(ERS_HERE, typeid(MessageType).name()); // NOLINT(runtime/rtti)
  }

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, bool>::type
  try_write_network(MessageType& message, Sender::timeout_t const& timeout)
  {
    if (m_network_sender_ptr == nullptr) {
      ers::error(ConnectionInstanceNotFound(ERS_HERE,to_string(this->endpoint())));
      return false;
    }

    auto serialized = dunedaq::serialization::serialize(message, dunedaq::serialization::kMsgPack);
    // TLOG() << "Serialized message for network sending: " << serialized.size() << ", this=" << (void*)this;
    std::lock_guard<std::mutex> lk(m_send_mutex);

    return m_network_sender_ptr->send(serialized.data(), serialized.size(), timeout, m_topic, true);
  }

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, bool>::type
  try_write_network(MessageType&, Sender::timeout_t const&, std::string const&)
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
