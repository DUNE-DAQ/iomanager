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

#include "utilities/NamedObject.hpp"
#include "logging/Logging.hpp"

namespace dunedaq::iomanager {

// Typeless
class Sender : public utilities::NamedObject
{
public:
  using timeout_t = std::chrono::milliseconds;
  static constexpr timeout_t s_block = timeout_t::max();
  static constexpr timeout_t s_no_block = timeout_t::zero();

  explicit Sender(Endpoint const& this_endpoint)
    : utilities::NamedObject(to_string(this_endpoint))
    , m_endpoint(this_endpoint)
  {}
  virtual ~Sender() = default;

  Endpoint endpoint() const { return m_endpoint; }

protected:
  Endpoint m_endpoint;
};

// Interface
template<typename Datatype>
class SenderConcept : public Sender
{
public:
  explicit SenderConcept(Endpoint const& this_endpoint)
    : Sender(this_endpoint)
  {}
  virtual void send(Datatype&& data, Sender::timeout_t timeout) = 0;     // NOLINT
  virtual bool try_send(Datatype&& data, Sender::timeout_t timeout) = 0; // NOLINT
};

} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
