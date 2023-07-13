/**
 * @file Sender.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_QSENDER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_QSENDER_HPP_

#include "iomanager/Sender.hpp"

#include <memory>
#include <string>

namespace dunedaq::iomanager {

// QImpl
template<typename Datatype>
class QueueSenderModel : public SenderConcept<Datatype>
{
public:
  explicit QueueSenderModel(ConnectionId const& request);

  QueueSenderModel(QueueSenderModel&& other);

  void send(Datatype&& data, Sender::timeout_t timeout) override;

  bool try_send(Datatype&& data, Sender::timeout_t timeout) override;

  void send_with_topic(Datatype&& data, Sender::timeout_t timeout, std::string) override;

private:
  std::shared_ptr<Queue<Datatype>> m_queue;
};

} // namespace dunedaq::iomanager

#include "detail/QueueSenderModel.hxx"

#endif // IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
