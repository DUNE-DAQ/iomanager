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

  explicit Sender(ConnectionId const& this_conn, std::string const& session)
    : utilities::NamedObject(this_conn.uid)
    , m_conn(this_conn)
      , m_session(session)
  {
  }
  virtual ~Sender() = default;

  ConnectionId id() const { return m_conn; }
  std::string session() const { return m_session; }

protected:
  ConnectionId m_conn;
  std::string m_session;
};

// Interface
template<typename Datatype>
class SenderConcept : public Sender
{
public:
  explicit SenderConcept(ConnectionId const& conn_id, std::string const& session)
    : Sender(conn_id, session)
  {}
  virtual void send(Datatype&& data, Sender::timeout_t timeout) = 0;     // NOLINT
  virtual bool try_send(Datatype&& data, Sender::timeout_t timeout) = 0; // NOLINT
  virtual void send_with_topic(Datatype&& data, Sender::timeout_t timeout, std::string topic) = 0; // NOLINT
};

} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
