/**
 * @file IOManager.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_

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

/**
 * @class IOManager
 * Wrapper class for sockets and SPSC circular buffers.
 *   Makes the communication between DAQ processes easier and scalable.
 */
class IOManager
{

public:
  static std::shared_ptr<IOManager> get()
  {
    if (!s_instance)
      s_instance = std::shared_ptr<IOManager>(new IOManager());

    return s_instance;
  }

  IOManager(const IOManager&) = delete;            ///< IOManager is not copy-constructible
  IOManager& operator=(const IOManager&) = delete; ///< IOManager is not copy-assignable
  IOManager(IOManager&&) = delete;                 ///< IOManager is not move-constructible
  IOManager& operator=(IOManager&&) = delete;      ///< IOManager is not move-assignable

  void configure(Queues_t queues, Connections_t connections, bool use_config_client, std::chrono::milliseconds config_client_interval)
  {
    Queues_t qCfg = queues;
    Connections_t nwCfg;

    for (auto& connection : connections) {
      nwCfg.push_back(connection);
    }

    QueueRegistry::get().configure(qCfg);
    NetworkManager::get().configure(nwCfg, use_config_client, config_client_interval);
  }

  void reset()
  {
    QueueRegistry::get().reset();
    NetworkManager::get().reset();
    m_senders.clear();
    m_receivers.clear();
    s_instance = nullptr;
  }

  template<typename Datatype>
  std::shared_ptr<SenderConcept<Datatype>> get_sender(ConnectionId const& id)
  {
    if (id.data_type != datatype_to_string<Datatype>()) {
      throw DatatypeMismatch(ERS_HERE, id.uid, id.data_type, datatype_to_string<Datatype>());
    }

    static std::mutex dt_sender_mutex;
    std::lock_guard<std::mutex> lk(dt_sender_mutex);

    if (!m_senders.count(id)) {
      if (QueueRegistry::get().has_queue(id.uid, id.data_type)) { // if queue
        TLOG() << "Creating QueueSenderModel for uid " << id.uid << ", datatype " << id.data_type;
        m_senders[id] = std::make_shared<QueueSenderModel<Datatype>>(QueueSenderModel<Datatype>(id.uid));
      } else {
        TLOG() << "Creating NetworkSenderModel for uid " << id.uid << ", datatype " << id.data_type;
        m_senders[id] = std::make_shared<NetworkSenderModel<Datatype>>(NetworkSenderModel<Datatype>(id.uid));
      }
    }
    return std::dynamic_pointer_cast<SenderConcept<Datatype>>(m_senders[id]);
  }

  template<typename Datatype>
  std::shared_ptr<SenderConcept<Datatype>> get_sender(std::string const& uid)
  {
    auto data_type = datatype_to_string<Datatype>();
    ConnectionId id;
    id.uid = uid;
    id.data_type = data_type;
    return get_sender<Datatype>(id);
  }

  template<typename Datatype>
  std::shared_ptr<ReceiverConcept<Datatype>> get_receiver(ConnectionId const& id)
  {
    if (id.data_type != datatype_to_string<Datatype>()) {
      throw DatatypeMismatch(ERS_HERE, id.uid, id.data_type, datatype_to_string<Datatype>());
    }

    static std::mutex dt_receiver_mutex;
    std::lock_guard<std::mutex> lk(dt_receiver_mutex);

    if (!m_receivers.count(id)) {
      if (QueueRegistry::get().has_queue(id.uid, id.data_type)) { // if queue
        TLOG() << "Creating QueueReceiverModel for uid " << id.uid << ", datatype " << id.data_type;
        m_receivers[id] = std::make_shared<QueueReceiverModel<Datatype>>(QueueReceiverModel<Datatype>(id.uid));
      } else {
        TLOG() << "Creating NetworkReceiverModel for uid " << id.uid << ", datatype " << id.data_type;
        m_receivers[id] = std::make_shared<NetworkReceiverModel<Datatype>>(NetworkReceiverModel<Datatype>(id.uid));
      }
    }
    return std::dynamic_pointer_cast<ReceiverConcept<Datatype>>(m_receivers[id]); // NOLINT
  }

  template<typename Datatype>
  std::shared_ptr<ReceiverConcept<Datatype>> get_receiver(std::string const& uid)
  {
    auto data_type = datatype_to_string<Datatype>();
    ConnectionId id;
    id.uid = uid;
    id.data_type = data_type;
    return get_receiver<Datatype>(id);
  }

  template<typename Datatype>
  void add_callback(ConnectionId const& id, std::function<void(Datatype&)> callback)
  {
    auto receiver = get_receiver<Datatype>(id);
    receiver->add_callback(callback);
  }

  template<typename Datatype>
  void add_callback(std::string const& uid, std::function<void(Datatype&)> callback)
  {
    auto receiver = get_receiver<Datatype>(uid);
    receiver->add_callback(callback);
  }

  template<typename Datatype>
  void remove_callback(ConnectionId const& id)
  {
    auto receiver = get_receiver<Datatype>(id);
    receiver->remove_callback();
  }

  template<typename Datatype>
  void remove_callback(std::string const& uid)
  {
    auto receiver = get_receiver<Datatype>(uid);
    receiver->remove_callback();
  }

  std::set<std::string> get_datatypes(std::string const& uid)
  {
    auto output = QueueRegistry::get().get_datatypes(uid);
    auto networks = NetworkManager::get().get_datatypes(uid);
    for (auto& dt : networks) {
      output.insert(dt);
    }
    return output;
  }

private:
  IOManager() {}

  using SenderMap = std::map<ConnectionId, std::shared_ptr<Sender>>;
  using ReceiverMap = std::map<ConnectionId, std::shared_ptr<Receiver>>;
  SenderMap m_senders;
  ReceiverMap m_receivers;

  static std::shared_ptr<IOManager> s_instance;
};

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

#endif // IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_
