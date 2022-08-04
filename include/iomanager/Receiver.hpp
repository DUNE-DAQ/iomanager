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

  explicit Receiver(ConnectionRequest const& this_req)
    : utilities::NamedObject(to_string(this_req))
    , m_request(this_req)
  {}
  virtual ~Receiver() = default;

  ConnectionRequest request() const { return m_request; }

protected:
  ConnectionRequest m_request;
};

// Interface
template<typename Datatype>
class ReceiverConcept : public Receiver
{
public:
  explicit ReceiverConcept(ConnectionRequest const& this_req)
    : Receiver(this_req)
  {}
  virtual Datatype receive(Receiver::timeout_t timeout) = 0;
  virtual std::optional<Datatype> try_receive(Receiver::timeout_t timeout) = 0;
  virtual void add_callback(std::function<void(Datatype&)> callback) = 0;
  virtual void remove_callback() = 0;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_
