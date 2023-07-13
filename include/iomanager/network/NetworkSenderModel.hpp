/**
 * @file Sender.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_NSENDER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_NSENDER_HPP_

#include "iomanager/Sender.hpp"

#include "ipm/Sender.hpp"
#include "serialization/Serialization.hpp"

#include <memory>
#include <string>
#include <utility>

namespace dunedaq::iomanager {

// NImpl
template<typename Datatype>
class NetworkSenderModel : public SenderConcept<Datatype>
{
public:
  using SenderConcept<Datatype>::send;

  explicit NetworkSenderModel(ConnectionId const& conn_id);

  NetworkSenderModel(NetworkSenderModel&& other);

  void send(Datatype&& data, Sender::timeout_t timeout) override;

  bool try_send(Datatype&& data, Sender::timeout_t timeout) override;

  void send_with_topic(Datatype&& data, Sender::timeout_t timeout, std::string topic) override;

private:
  void get_sender(Sender::timeout_t const& timeout);

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, void>::type write_network(
    MessageType& message,
    Sender::timeout_t const& timeout);

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, void>::type write_network(
    MessageType&,
    Sender::timeout_t const&);

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, bool>::type try_write_network(
    MessageType& message,
    Sender::timeout_t const& timeout);

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, bool>::type try_write_network(
    MessageType&,
    Sender::timeout_t const&);

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, void>::type write_network_with_topic(
    MessageType& message,
    Sender::timeout_t const& timeout, std::string topic);

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, void>::type
  write_network_with_topic(
    MessageType&,
    Sender::timeout_t const&, std::string);
  
  Sender::timeout_t extend_first_timeout(Sender::timeout_t timeout);

  std::shared_ptr<ipm::Sender> m_network_sender_ptr;
  std::mutex m_send_mutex;
  std::string m_topic{ "" };
  std::atomic<bool> m_first{ true };
};

} // namespace dunedaq::iomanager

#include "detail/NetworkSenderModel.hxx"

#endif // IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
