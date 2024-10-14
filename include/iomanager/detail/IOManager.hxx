#include "iomanager/connection/Structs.hpp"
#include "iomanager/network/ConfigClient.hpp"
#include "iomanager/network/NetworkManager.hpp"
#include "iomanager/network/NetworkReceiverModel.hpp"
#include "iomanager/network/NetworkSenderModel.hpp"
#include "iomanager/queue/QueueReceiverModel.hpp"
#include "iomanager/queue/QueueRegistry.hpp"
#include "iomanager/queue/QueueSenderModel.hpp"

#include "nlohmann/json.hpp"

#include <cstdlib>
#include <map>
#include <memory>
#include <regex>
#include <string>

namespace dunedaq {

namespace iomanager {

using namespace connection;

template<typename Datatype>
inline void
IOManager::add_callback(ConnectionId const& id, std::function<void(Datatype&)> callback)
{
  auto receiver = get_receiver<Datatype>(id);
  receiver->add_callback(callback);
}

template<typename Datatype>
inline std::shared_ptr<ReceiverConcept<Datatype>>
IOManager::get_receiver(std::string const& uid)
{
  auto data_type = datatype_to_string<Datatype>();
  ConnectionId id;
  id.uid = uid;
  id.data_type = data_type;
  id.system = m_system;
  return get_receiver<Datatype>(id);
}

template<typename Datatype>
inline std::shared_ptr<ReceiverConcept<Datatype>>
IOManager::get_receiver(ConnectionId id)
{
  if (id.data_type != datatype_to_string<Datatype>()) {
    throw DatatypeMismatch(ERS_HERE, id.uid, id.data_type, datatype_to_string<Datatype>());
  }

  if (id.system == "") {
    id.system = m_system;
  }

  static std::mutex dt_receiver_mutex;
  std::lock_guard<std::mutex> lk(dt_receiver_mutex);

  if (!m_receivers.count(id)) {
    if (QueueRegistry::get().has_queue(id.uid, id.data_type)) { // if queue
      TLOG("IOManager") << "Creating QueueReceiverModel for uid " << id.uid << ", datatype " << id.data_type;
      m_receivers[id] = std::make_shared<QueueReceiverModel<Datatype>>(id);
    } else {
      TLOG("IOManager") << "Creating NetworkReceiverModel for uid " << id.uid << ", datatype " << id.data_type
                        << " in system " << id.system;
      m_receivers[id] = std::make_shared<NetworkReceiverModel<Datatype>>(id);
    }
  }
  return std::dynamic_pointer_cast<ReceiverConcept<Datatype>>(m_receivers[id]); // NOLINT
}

template<typename Datatype>
inline std::shared_ptr<SenderConcept<Datatype>>
IOManager::get_sender(std::string const& uid)
{
  auto data_type = datatype_to_string<Datatype>();
  ConnectionId id;
  id.uid = uid;
  id.data_type = data_type;
  id.system = m_system;
  return get_sender<Datatype>(id);
}

template<typename Datatype>
inline std::shared_ptr<SenderConcept<Datatype>>
IOManager::get_sender(ConnectionId id)
{
  if (id.data_type != datatype_to_string<Datatype>()) {
    throw DatatypeMismatch(ERS_HERE, id.uid, id.data_type, datatype_to_string<Datatype>());
  }

  if (id.system == "") {
    id.system = m_system;
  }

  static std::mutex dt_sender_mutex;
  std::lock_guard<std::mutex> lk(dt_sender_mutex);

  if (!m_senders.count(id)) {
    if (QueueRegistry::get().has_queue(id.uid, id.data_type)) { // if queue
      TLOG("IOManager") << "Creating QueueSenderModel for uid " << id.uid << ", datatype " << id.data_type;
      m_senders[id] = std::make_shared<QueueSenderModel<Datatype>>(id);
    } else {
      TLOG("IOManager") << "Creating NetworkSenderModel for uid " << id.uid << ", datatype " << id.data_type
                        << " in system " << id.system;
      m_senders[id] = std::make_shared<NetworkSenderModel<Datatype>>(id);
    }
  }
  return std::dynamic_pointer_cast<SenderConcept<Datatype>>(m_senders[id]);
}

template<typename Datatype>
inline void
IOManager::add_callback(std::string const& uid, std::function<void(Datatype&)> callback)
{
  auto receiver = get_receiver<Datatype>(uid);
  receiver->add_callback(callback);
}

template<typename Datatype>
inline void
IOManager::remove_callback(ConnectionId const& id)
{
  auto receiver = get_receiver<Datatype>(id);
  receiver->remove_callback();
}

template<typename Datatype>
inline void
IOManager::remove_callback(std::string const& uid)
{
  auto receiver = get_receiver<Datatype>(uid);
  receiver->remove_callback();
}

} // namespace iomanager

// Helper functions
[[maybe_unused]] static std::shared_ptr<iomanager::IOManager> // NOLINT(build/namespaces)
get_iomanager()
{
  return iomanager::IOManager::get();
}

template<typename Datatype>
static std::shared_ptr<iomanager::SenderConcept<Datatype>> // NOLINT(build/namespaces)
get_iom_sender(iomanager::ConnectionId const& id)
{
  return iomanager::IOManager::get()->get_sender<Datatype>(id);
}

template<typename Datatype>
static std::shared_ptr<iomanager::ReceiverConcept<Datatype>> // NOLINT(build/namespaces)
get_iom_receiver(iomanager::ConnectionId const& id)
{
  return iomanager::IOManager::get()->get_receiver<Datatype>(id);
}

template<typename Datatype>
static std::shared_ptr<iomanager::SenderConcept<Datatype>> // NOLINT(build/namespaces)
get_iom_sender(std::string const& uid)
{
  return iomanager::IOManager::get()->get_sender<Datatype>(uid);
}

template<typename Datatype>
static std::shared_ptr<iomanager::ReceiverConcept<Datatype>> // NOLINT(build/namespaces)
get_iom_receiver(std::string const& uid)
{
  return iomanager::IOManager::get()->get_receiver<Datatype>(uid);
}
} // namespace dunedaq
