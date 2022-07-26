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

#include "utilities/NamedObject.hpp"
#include "logging/Logging.hpp"

namespace dunedaq {

namespace iomanager {

// Typeless
class Receiver : public utilities::NamedObject
{
public:
  using timeout_t = std::chrono::milliseconds;
  static constexpr timeout_t s_block = timeout_t::max();
  static constexpr timeout_t s_no_block = timeout_t::zero();

  explicit Receiver(Endpoint const& this_endpoint)
    : utilities::NamedObject(to_string(this_endpoint))
    , m_endpoint(this_endpoint)
  {}
  virtual ~Receiver() = default;

  Endpoint endpoint() const { return m_endpoint; }

protected:
  Endpoint m_endpoint;
};

// Interface
template<typename Datatype>
class ReceiverConcept : public Receiver
{
public:
  explicit ReceiverConcept(Endpoint const& this_endpoint)
    : Receiver(this_endpoint)
  {}
  virtual Datatype receive(Receiver::timeout_t timeout) = 0;
  virtual std::optional<Datatype> try_receive(Receiver::timeout_t timeout) = 0;
  virtual void add_callback(std::function<void(Datatype&)> callback) = 0;
  virtual void remove_callback() = 0;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_
