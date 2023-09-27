
#include "iomanager/Sender.hpp"
#include "iomanager/queue/QueueIssues.hpp"
#include "iomanager/queue/QueueRegistry.hpp"

#include "logging/Logging.hpp"

#include <memory>
#include <string>
#include <typeinfo>
#include <utility>

namespace dunedaq::iomanager {
	
template<typename Datatype>
inline void
QueueSenderModel<Datatype>::send_with_topic(Datatype&& data, Sender::timeout_t timeout, std::string) // NOLINT
{
  // Topics are not used for Queues
  send(std::move(data), timeout);
}

template<typename Datatype>
inline bool
QueueSenderModel<Datatype>::try_send(Datatype&& data, Sender::timeout_t timeout) // NOLINT
{
  if (m_queue == nullptr) {
    ers::error(ConnectionInstanceNotFound(ERS_HERE, this->id().uid));
    return false;
  }

  return m_queue->try_push(std::move(data), timeout);
}

template<typename Datatype>
inline void
QueueSenderModel<Datatype>::send(Datatype&& data, Sender::timeout_t timeout) // NOLINT
{
  if (m_queue == nullptr)
    throw ConnectionInstanceNotFound(ERS_HERE, this->id().uid);

  try {
    m_queue->push(std::move(data), timeout);
  } catch (QueueTimeoutExpired& ex) {
    throw TimeoutExpired(ERS_HERE, this->id().uid, "push", timeout.count(), ex);
  }
}

template<typename Datatype>
inline bool
QueueSenderModel<Datatype>::is_ready_for_send(Sender::timeout_t timeout) // NOLINT
{
  // Queues are always ready
  return true;
}

template<typename Datatype>
inline QueueSenderModel<Datatype>::QueueSenderModel(QueueSenderModel&& other)
  : SenderConcept<Datatype>(other.m_conn.uid)
  , m_queue(std::move(other.m_queue))
{
}

template<typename Datatype>
inline QueueSenderModel<Datatype>::QueueSenderModel(ConnectionId const& request)
  : SenderConcept<Datatype>(request)
{
  TLOG() << "QueueSenderModel created with DT! Addr: " << static_cast<void*>(this);
  m_queue = QueueRegistry::get().get_queue<Datatype>(request.uid);
  TLOG() << "QueueSenderModel m_queue=" << static_cast<void*>(m_queue.get());
  // get queue ref from queueregistry based on conn_id
}

} // namespace dunedaq::iomanager
