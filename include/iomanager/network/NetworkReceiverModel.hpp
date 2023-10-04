/**
 * @file Receiver.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_NRECEIVER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_NRECEIVER_HPP_

#include "iomanager/Receiver.hpp"

#include "ipm/Subscriber.hpp"
#include "serialization/Serialization.hpp"


namespace dunedaq {

namespace iomanager {

// NImpl
template<typename Datatype>
class NetworkReceiverModel : public ReceiverConcept<Datatype>
{
public:
  explicit NetworkReceiverModel(ConnectionId const& conn_id);
  ~NetworkReceiverModel() { remove_callback(); }

  NetworkReceiverModel(NetworkReceiverModel&& other);

  Datatype receive(Receiver::timeout_t timeout) override;

  std::optional<Datatype> try_receive(Receiver::timeout_t timeout) override
  {
    return try_read_network<Datatype>(timeout);
  }
  void add_callback(std::function<void(Datatype&)> callback) override { add_callback_impl<Datatype>(callback); }

  void remove_callback() override;

  void subscribe(std::string topic) override;
  void unsubscribe(std::string topic) override;

private:
  void get_receiver(Receiver::timeout_t timeout);

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, MessageType>::type read_network(
    Receiver::timeout_t const& timeout);

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, MessageType>::type read_network(
    Receiver::timeout_t const&);

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, std::optional<MessageType>>::type
  try_read_network(Receiver::timeout_t const& timeout);

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value,
                          std::optional<MessageType>>::type
  try_read_network(Receiver::timeout_t const&);

  template<typename MessageType>
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, void>::type add_callback_impl(
    std::function<void(MessageType&)> callback);

  template<typename MessageType>
  typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, void>::type add_callback_impl(
    std::function<void(MessageType&)>);

  std::atomic<bool> m_with_callback{ false };
  std::function<void(Datatype&)> m_callback;
  std::unique_ptr<std::thread> m_event_loop_runner;
  std::shared_ptr<ipm::Receiver> m_network_receiver_ptr{ nullptr };
  std::mutex m_callback_mutex;
  std::mutex m_receive_mutex;
};

} // namespace iomanager
} // namespace dunedaq

#include "detail/NetworkReceiverModel.hxx"

#endif // IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_
