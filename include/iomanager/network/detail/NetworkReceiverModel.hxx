#include "iomanager/Receiver.hpp"
#include "iomanager/network/NetworkIssues.hpp"
#include "iomanager/network/NetworkManager.hpp"

#include "ipm/Subscriber.hpp"
#include "logging/Logging.hpp"
#include "serialization/Serialization.hpp"
#include "utilities/ReusableThread.hpp"

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <typeinfo>
#include <utility>

namespace dunedaq {

namespace iomanager {

template<typename Datatype>
inline NetworkReceiverModel<Datatype>::NetworkReceiverModel(ConnectionId const& conn_id)
  : ReceiverConcept<Datatype>(conn_id)
{
  TLOG() << "NetworkReceiverModel created with DT! ID: " << conn_id.uid << " Addr: " << static_cast<void*>(this);
  try {
    get_receiver(std::chrono::milliseconds(1000));
  } catch (ConnectionNotFound const& ex) {
    TLOG() << "Initial connection attempt failed: " << ex;
  }
}

template<typename Datatype>
inline NetworkReceiverModel<Datatype>::NetworkReceiverModel(NetworkReceiverModel&& other)
  : ReceiverConcept<Datatype>(other.m_conn.uid)
  , m_with_callback(other.m_with_callback.load())
  , m_callback(std::move(other.m_callback))
  , m_event_loop_runner(std::move(other.m_event_loop_runner))
  , m_network_receiver_ptr(std::move(other.m_network_receiver_ptr))
{
}

template<typename Datatype>
inline Datatype
NetworkReceiverModel<Datatype>::receive(Receiver::timeout_t timeout)
{
  try {
    return read_network<Datatype>(timeout);
  } catch (ipm::ReceiveTimeoutExpired& ex) {
    throw TimeoutExpired(ERS_HERE, this->id().uid, "receive", timeout.count(), ex);
  }
}

template<typename Datatype>
inline void
NetworkReceiverModel<Datatype>::remove_callback()
{
  std::lock_guard<std::mutex> lk(m_callback_mutex);
  m_with_callback = false;
  if (m_event_loop_runner != nullptr && m_event_loop_runner->joinable()) {
    m_event_loop_runner->join();
    m_event_loop_runner.reset(nullptr);
  } else if (m_event_loop_runner != nullptr) {
    TLOG() << "Event loop can't be closed!";
  }
  // remove function.
}

template<typename Datatype>
inline void
NetworkReceiverModel<Datatype>::subscribe(std::string topic)
{
  if (NetworkManager::get().is_pubsub_connection(this->m_conn)) {
    std::dynamic_pointer_cast<ipm::Subscriber>(m_network_receiver_ptr)->subscribe(topic);
  }
}

template<typename Datatype>
inline void
NetworkReceiverModel<Datatype>::unsubscribe(std::string topic)
{
  if (NetworkManager::get().is_pubsub_connection(this->m_conn)) {
    std::dynamic_pointer_cast<ipm::Subscriber>(m_network_receiver_ptr)->unsubscribe(topic);
  }
}

template<typename Datatype>
inline void
NetworkReceiverModel<Datatype>::get_receiver(Receiver::timeout_t timeout)
{
  // get network resources
  auto start = std::chrono::steady_clock::now();
  while (m_network_receiver_ptr == nullptr &&
         std::chrono::duration_cast<Receiver::timeout_t>(std::chrono::steady_clock::now() - start) < timeout) {

    try {
      m_network_receiver_ptr = NetworkManager::get().get_receiver(this->id());
    } catch (ConnectionNotFound const& ex) {
      m_network_receiver_ptr = nullptr;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
}

template<typename Datatype>
template<typename MessageType>
inline typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, MessageType>::type
NetworkReceiverModel<Datatype>::read_network(Receiver::timeout_t const& timeout)
{
  std::lock_guard<std::mutex> lk(m_receive_mutex);
  get_receiver(timeout);

  if (m_network_receiver_ptr == nullptr) {
    throw ConnectionInstanceNotFound(ERS_HERE, this->id().uid);
  }

  auto response = m_network_receiver_ptr->receive(timeout);
  if (response.data.size() > 0) {
    return dunedaq::serialization::deserialize<MessageType>(response.data);
  }

  throw TimeoutExpired(ERS_HERE, this->id().uid, "network receive", timeout.count());
  return MessageType();
}

template<typename Datatype>
template<typename MessageType>
inline typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, MessageType>::type
NetworkReceiverModel<Datatype>::read_network(Receiver::timeout_t const&)
{
  throw NetworkMessageNotSerializable(ERS_HERE, typeid(MessageType).name()); // NOLINT(runtime/rtti)
  return MessageType();
}

template<typename Datatype>
template<typename MessageType>
inline
  typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, std::optional<MessageType>>::type
  NetworkReceiverModel<Datatype>::try_read_network(Receiver::timeout_t const& timeout)
{
  std::lock_guard<std::mutex> lk(m_receive_mutex);
  get_receiver(timeout);
  if (m_network_receiver_ptr == nullptr) {
    TLOG() << ConnectionInstanceNotFound(ERS_HERE, this->id().uid);
    return std::nullopt;
  }

  ipm::Receiver::Response res;
  res = m_network_receiver_ptr->receive(timeout, ipm::Receiver::s_any_size, true);

  if (res.data.size() > 0) {
    return std::make_optional<MessageType>(dunedaq::serialization::deserialize<MessageType>(res.data));
  }

  return std::nullopt;
}

template<typename Datatype>
template<typename MessageType>
inline typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value,
                               std::optional<MessageType>>::type
NetworkReceiverModel<Datatype>::try_read_network(Receiver::timeout_t const&)
{
  ers::error(NetworkMessageNotSerializable(ERS_HERE, typeid(MessageType).name())); // NOLINT(runtime/rtti)
  return std::nullopt;
}

template<typename Datatype>
template<typename MessageType>
inline typename std::enable_if<dunedaq::serialization::is_serializable<MessageType>::value, void>::type
NetworkReceiverModel<Datatype>::add_callback_impl(std::function<void(MessageType&)> callback)
{
  remove_callback();
  {
    std::lock_guard<std::mutex> lk(m_callback_mutex);
  }
  TLOG() << "Registering callback.";
  m_callback = callback;
  m_with_callback = true;
  // start event loop (thread that calls when receive happens)
  m_event_loop_runner = std::make_unique<std::thread>([&]() {
    std::optional<Datatype> message;
    while (m_with_callback.load() || message) {
      try {
        message = try_read_network<Datatype>(std::chrono::milliseconds(20));
        if (message) {
          m_callback(*message);
        }
      } catch (const ers::Issue&) {
        ;
      }
    }
  });
}

template<typename Datatype>
template<typename MessageType>
inline typename std::enable_if<!dunedaq::serialization::is_serializable<MessageType>::value, void>::type
NetworkReceiverModel<Datatype>::add_callback_impl(std::function<void(MessageType&)>)
{
  throw NetworkMessageNotSerializable(ERS_HERE, typeid(MessageType).name()); // NOLINT(runtime/rtti)
}

} // namespace iomanager
} // namespace dunedaq
