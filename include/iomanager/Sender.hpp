/**
 * @file Sender.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_

#include "iomanager/ConnectionId.hpp"

#include "appfwk/QueueRegistry.hpp"
#include "serialization/Serialization.hpp"
#include "ipm/Sender.hpp"
#include "networkmanager/NetworkManager.hpp"

#include <any>
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>

namespace dunedaq {
namespace iomanager {

// Typeless
class Sender
{
public:
  using timeout_t = std::chrono::milliseconds;
  static constexpr timeout_t s_block = timeout_t::max();
  static constexpr timeout_t s_no_block = timeout_t::zero();

  virtual ~Sender() = default;
};

// Interface
template<typename Datatype>
class SenderConcept : public Sender
{
public:
  virtual void send(Datatype& data, Sender::timeout_t timeout, Topic_t topic = "") = 0;
};

// QImpl
template<typename Datatype>
class QueueSenderModel : public SenderConcept<Datatype>
{
public:
  explicit QueueSenderModel(ConnectionId conn_id, ConnectionRef conn_ref)
    : m_conn_id(conn_id)
    , m_conn_ref(conn_ref)
  {
    TLOG() << "QueueSenderModel created with DT! Addr: " << (void*)this; 
    m_queue= appfwk::QueueRegistry::get().get_queue<Datatype>(conn_id.uid);
    TLOG() << "QueueSenderModel m_queue=" << (void*)m_queue.get();
    // get queue ref from queueregistry based on conn_id
  }

  void send(Datatype& data, Sender::timeout_t timeout, Topic_t topic = "") override
  {
    if (topic != "") {
      TLOG() << "Topics are invalid for queues! Check config!";
    }
    //TLOG() << "Handle data: " << data;
    m_queue->push(std::move(data), timeout);
    // if (m_queue->write(
  }

  ConnectionId m_conn_id;
  ConnectionRef m_conn_ref;
  std::shared_ptr<appfwk::Queue<Datatype>> m_queue;
};

// NImpl
template<typename Datatype>
class NetworkSenderModel : public SenderConcept<Datatype>
{
public:
  using SenderConcept<Datatype>::send;

  explicit NetworkSenderModel(ConnectionId conn_id, ConnectionRef conn_ref)
    : m_conn_id(conn_id)
    , m_conn_ref(conn_ref)
  {
    TLOG() << "NetworkSenderModel created with DT! Addr: " << (void*)this;
    // get network resources
    m_network_sender_ptr = networkmanager::NetworkManager::get().get_sender(conn_id.uid);
  }

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, void>::type
  write_network(MessageType& message, Sender::timeout_t const& timeout, std::string const& topic = "")
  {
    if (m_network_sender_ptr == nullptr)
      return;
    auto serialized = dunedaq::serialization::serialize(message, dunedaq::serialization::kMsgPack);
    //TLOG() << "Serialized message for network sending: " << serialized.size();
    m_network_sender_ptr->send(serialized.data(), serialized.size(), timeout, topic);
  }

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, void>::type
  write_network(MessageType&, Sender::timeout_t const&, std::string const&)
  {
    TLOG() << "Not sending non-serializable message!";
  }

  void send(Datatype& data, Sender::timeout_t timeout, Topic_t topic = "") override
  {
    write_network<Datatype>(data, timeout, topic);
  }

  ConnectionId m_conn_id;
  ConnectionRef m_conn_ref;
  std::shared_ptr<ipm::Sender> m_network_sender_ptr;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
