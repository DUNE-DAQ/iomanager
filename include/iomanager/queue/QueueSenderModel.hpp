/**
 * @file Sender.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_QSENDER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_QSENDER_HPP_

#include "iomanager/CommonIssues.hpp"
#include "iomanager/Sender.hpp"
#include "iomanager/queue/QueueRegistry.hpp"

#include "logging/Logging.hpp"

#include <memory>
#include <string>
#include <typeinfo>
#include <utility>

namespace dunedaq::iomanager {

// QImpl
template<typename Datatype>
class QueueSenderModel : public SenderConcept<Datatype>
{
public:
  explicit QueueSenderModel(ConnectionRequest const& request)
    : SenderConcept<Datatype>(request)
  {
    TLOG() << "QueueSenderModel created with DT! Addr: " << static_cast<void*>(this);
    m_queue = QueueRegistry::get().get_queue<Datatype>(request);
    TLOG() << "QueueSenderModel m_queue=" << static_cast<void*>(m_queue.get());
    // get queue ref from queueregistry based on conn_id
  }

  QueueSenderModel(QueueSenderModel&& other)
    : SenderConcept<Datatype>(other.m_request)
    , m_queue(std::move(other.m_queue))
  {}

  void send(Datatype&& data, Sender::timeout_t timeout) override // NOLINT
  {
    if (m_queue == nullptr)
      throw ConnectionInstanceNotFound(ERS_HERE, to_string(this->request()));

    try {
      m_queue->push(std::move(data), timeout);
    } catch (QueueTimeoutExpired& ex) {
      throw TimeoutExpired(ERS_HERE, to_string(this->request()), "push", timeout.count(), ex);
    }
  }

  bool try_send(Datatype&& data, Sender::timeout_t timeout) override // NOLINT
  {
    if (m_queue == nullptr) {
      ers::error(ConnectionInstanceNotFound(ERS_HERE, to_string(this->request())));
      return false;
    }

    return m_queue->try_push(std::move(data), timeout);
  }

private:
  std::shared_ptr<Queue<Datatype>> m_queue;
};

} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_SENDER_HPP_
