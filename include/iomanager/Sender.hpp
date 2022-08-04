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

  explicit Sender(ConnectionRequest const& this_req)
    : utilities::NamedObject(to_string(this_req))
    , m_request(this_req)
  {}
  virtual ~Sender() = default;

  ConnectionRequest request() const { return m_request; }

protected:
  ConnectionRequest m_request;
};

// Interface
template<typename Datatype>
class SenderConcept : public Sender
{
public:
  explicit SenderConcept(ConnectionRequest const& this_req)
    : Sender(this_req)
  {}
  virtual void send(Datatype&& data, Sender::timeout_t timeout) = 0;     // NOLINT
  virtual bool try_send(Datatype&& data, Sender::timeout_t timeout) = 0; // NOLINT
};

} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
