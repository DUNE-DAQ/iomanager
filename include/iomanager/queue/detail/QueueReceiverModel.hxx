
#include "iomanager/Receiver.hpp"
#include "iomanager/queue/QueueIssues.hpp"
#include "iomanager/queue/QueueRegistry.hpp"

#include "logging/Logging.hpp"
#include "utilities/Issues.hpp"

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <typeinfo>
#include <utility>

namespace dunedaq::iomanager {

template<typename Datatype>
inline QueueReceiverModel<Datatype>::QueueReceiverModel(ConnectionId const& request)
  : ReceiverConcept<Datatype>(request)
{
  TLOG() << "QueueReceiverModel created with DT! Addr: " << this;
  // get queue ref from queueregistry based on conn_id
  // std::string sink_name = conn_id to sink_name;
  // m_source = std::make_unique<appfwk::DAQSource<Datatype>>(sink_name);
  m_queue = QueueRegistry::get().get_queue<Datatype>(request.uid);
  TLOG() << "QueueReceiverModel m_queue=" << static_cast<void*>(m_queue.get());
}

template<typename Datatype>
inline QueueReceiverModel<Datatype>::QueueReceiverModel(QueueReceiverModel&& other)
  : ReceiverConcept<Datatype>(other.m_conn.uid)
  , m_callback(std::move(other.m_callback))
  , m_event_loop_runner(std::move(other.m_event_loop_runner))
  , m_queue(std::move(other.m_queue))
{
}

template<typename Datatype>
inline Datatype
QueueReceiverModel<Datatype>::receive(Receiver::timeout_t timeout)
{
  if (m_event_loop_runner != nullptr) {
    TLOG() << "QueueReceiver model is equipped with callback! Ignoring receive call.";
    throw ReceiveCallbackConflict(ERS_HERE, this->id().uid);
  }
  if (m_queue == nullptr) {
    throw ConnectionInstanceNotFound(ERS_HERE, this->id().uid);
  }
  // TLOG() << "Hand off data...";
  Datatype dt;
  try {
    m_queue->pop(dt, timeout);
  } catch (QueueTimeoutExpired& ex) {
    throw TimeoutExpired(ERS_HERE, this->id().uid, "pop", timeout.count(), ex);
  }
  return dt;
  // if (m_queue->write(
}

template<typename Datatype>
inline std::optional<Datatype>
QueueReceiverModel<Datatype>::try_receive(Receiver::timeout_t timeout)
{
  if (m_event_loop_runner != nullptr) {
    TLOG() << "QueueReceiver model is equipped with callback! Ignoring receive call.";
    ers::error(ReceiveCallbackConflict(ERS_HERE, this->id().uid));
    return std::nullopt;
  }
  if (m_queue == nullptr) {
    ers::error(ConnectionInstanceNotFound(ERS_HERE, this->id().uid));
    return std::nullopt;
  }
  // TLOG() << "Hand off data...";
  Datatype dt;
  auto ret = m_queue->try_pop(dt, timeout);
  if (ret) {
    return std::make_optional(std::move(dt));
  }
  return std::nullopt;
  // if (m_queue->write(
}

template<typename Datatype>
inline void
QueueReceiverModel<Datatype>::add_callback(std::function<void(Datatype&)> callback)
{
  remove_callback();
  TLOG() << "Registering callback.";
  m_callback = callback;
  // start event loop (thread that calls when receive happens)
  m_event_loop_runner = std::make_unique<std::jthread>([&](std::stop_token token) {
    Datatype dt;
    bool ret = true;
    while (!token.stop_requested() || ret) {
      // TLOG() << "Take data from q then invoke callback...";
      ret = m_queue->try_pop(dt, token.stop_requested() ? std::chrono::milliseconds(0) : std::chrono::milliseconds(1));
      if (ret) {
        m_callback(dt);
      }
    }
  });
  auto handle = m_event_loop_runner->native_handle();
  std::string name = "Q_" + this->id().uid;
  name.resize(15);
  auto rc = pthread_setname_np(handle, name.c_str());
  if (rc != 0) {
    std::ostringstream s;
    s << "The name " << name << " provided for the thread is too long.";
    ers::warning(utilities::ThreadingIssue(ERS_HERE, s.str()));
  }
}

template<typename Datatype>
inline void
QueueReceiverModel<Datatype>::remove_callback()
{
  if (m_event_loop_runner != nullptr && m_event_loop_runner->joinable()) {
    m_event_loop_runner->request_stop();
    m_event_loop_runner->join();
    m_event_loop_runner.reset(nullptr);
  } else if (m_event_loop_runner != nullptr) {
    TLOG() << "Event loop can't be closed!";
  }
  // remove function.
}

} // namespace dunedaq::iomanager
