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
#include "iomanager/SchemaUtils.hpp"

#include "logging/Logging.hpp"
#include "utilities/NamedObject.hpp"

namespace dunedaq {

namespace iomanager {

// Typeless
class Receiver : public utilities::NamedObject
{
public:
  using timeout_t = std::chrono::milliseconds;
  static constexpr timeout_t s_block = timeout_t::max();
  static constexpr timeout_t s_no_block = timeout_t::zero();

  explicit Receiver(ConnectionId const& this_conn, std::string const& session)
    : utilities::NamedObject(this_conn.uid)
    , m_conn(this_conn)
      , m_session(session)
  {
  }

  virtual ~Receiver() = default;

  ConnectionId id() const { return m_conn; }
  std::string session() const { return m_session; }

protected:
  ConnectionId m_conn;
  std::string m_session;
};

// Interface
template<typename Datatype>
class ReceiverConcept : public Receiver
{
public:
  explicit ReceiverConcept(ConnectionId const& conn_id, std::string const& session)
    : Receiver(conn_id, session)
  {
  }
  virtual Datatype receive(Receiver::timeout_t timeout) = 0;
  virtual std::optional<Datatype> try_receive(Receiver::timeout_t timeout) = 0;
  virtual void add_callback(std::function<void(Datatype&)> callback) = 0;
  virtual void remove_callback() = 0;
  virtual void subscribe(std::string topic) = 0;
  virtual void unsubscribe(std::string topic) = 0;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_
