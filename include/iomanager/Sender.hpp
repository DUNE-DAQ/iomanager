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
  virtual ~Sender() = default;
};

// Interface
template<typename Datatype>
class SenderConcept : public Sender
{
public:
  virtual void send(Datatype& /*data*/) = 0;
};

// QImpl
template<typename Datatype>
class QueueSenderModel : public SenderConcept<Datatype>
{
public:
  explicit QueueSenderModel(ConnectionID conn_id)
    : m_conn_id(conn_id)
  {
    TLOG() << "QueueSenderModel created with DT! Addr: " << (void*)this ;
    // get queue ref from queueregistry based on conn_id
  }

  void send(Datatype& data) override
  {
    TLOG() << "Handle data: " << data ;
    // if (m_queue->write(
  }

  ConnectionID m_conn_id;
};

// NImpl
template<typename Datatype>
class NetworkSenderModel : public SenderConcept<Datatype>
{
public:
  using SenderConcept<Datatype>::send;

  explicit NetworkSenderModel(ConnectionID conn_id)
    : SenderConcept<Datatype>(conn_id)
    , m_conn_id(conn_id)
  {
    TLOG() << "NetworkSenderModel created with DT! Addr: " << (void*)this;
    // get network resources
  }

  void send(Datatype& data) override
  {
    TLOG() << "Handle data: " << data;
    // if (m_queue->write(
  }

  ConnectionID m_conn_id;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
