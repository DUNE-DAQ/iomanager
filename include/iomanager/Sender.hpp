/**
 * @file Sender.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_

#include "iomanager/ConnectionID.hpp"

#include "appfwk/QueueRegistry.hpp"

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
  virtual void send(Datatype& /*data*/, Sender::timeout_t /*timeout*/) = 0;
};

// QImpl
template<typename Datatype>
class QueueSenderModel : public SenderConcept<Datatype>
{
public:
  explicit QueueSenderModel(ConnectionID conn_id)
    : m_conn_id(conn_id)
  {
    TLOG() << "QueueSenderModel created with DT! Addr: " << (void*)this; 
    m_queue= appfwk::QueueRegistry::get().get_queue<Datatype>(conn_id.m_service_name);
    // get queue ref from queueregistry based on conn_id
  }

  void send(Datatype& data, Sender::timeout_t timeout) override
  {
    //TLOG() << "Handle data: " << data;
    m_queue->push(std::move(data), timeout);
    // if (m_queue->write(
  }

  ConnectionID m_conn_id;
  std::shared_ptr<appfwk::Queue<Datatype>> m_queue;
};

// NImpl
template<typename Datatype>
class NetworkSenderModel : public SenderConcept<Datatype>
{
public:
  using SenderConcept<Datatype>::send;

  explicit NetworkSenderModel(ConnectionID conn_id)
    : m_conn_id(conn_id)
  {
    TLOG() << "NetworkSenderModel created with DT! Addr: " << (void*)this;
    // get network resources
  }

  void send(Datatype& data, Sender::timeout_t /*timeout*/) override
  {
    //TLOG() << "Handle data: " << data;
    // if (m_queue->write(
  }

  ConnectionID m_conn_id;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
