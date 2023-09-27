/**
 * @file Sender.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_

#include "iomanager/CommonIssues.hpp"
#include "iomanager/SchemaUtils.hpp"

#include "logging/Logging.hpp"
#include "utilities/NamedObject.hpp"

namespace dunedaq::iomanager {

// Typeless
class Sender : public utilities::NamedObject
{
public:
  using timeout_t = std::chrono::milliseconds;
  static constexpr timeout_t s_block = timeout_t::max();
  static constexpr timeout_t s_no_block = timeout_t::zero();

  explicit Sender(ConnectionId const& this_conn)
    : utilities::NamedObject(this_conn.uid)
    , m_conn(this_conn)
  {
  }
  virtual ~Sender() = default;

  ConnectionId id() const { return m_conn; }

protected:
  ConnectionId m_conn;
};

// Interface
template<typename Datatype>
class SenderConcept : public Sender
{
public:
  explicit SenderConcept(ConnectionId const& conn_id)
    : Sender(conn_id)
  {}
  virtual void send(Datatype&& data, Sender::timeout_t timeout) = 0;     // NOLINT
  virtual bool try_send(Datatype&& data, Sender::timeout_t timeout) = 0; // NOLINT
  virtual void send_with_topic(Datatype&& data, Sender::timeout_t timeout, std::string topic) = 0; // NOLINT
  virtual bool is_ready_for_sending(Sender::timeout_t timeout) = 0;      // NOLINT
};

} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
