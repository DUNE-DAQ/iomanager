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

  void configure(Queues_t queues, Connections_t connections)
  {
    Queues_t qCfg = queues;
    Connections_t nwCfg;
    std::regex queue_uri_regex("queue://(\\w+):(\\d+)");

    for (auto& connection : connections) {
      if (connection.connection_type == ConnectionType::kQueue) {
        std::smatch sm;
        std::regex_match(connection.uri, sm, queue_uri_regex);
        QueueConfig config;
        config.name = to_string(connection.bind_endpoint) + "_Queue";
        config.endpoints.push_back(connection.bind_endpoint);
        for (auto& ep : connection.connected_endpoints) {
          config.endpoints.push_back(ep);
        }
        config.queue_type = string_to_queue_type(sm[1]);
        config.capacity = stoi(sm[2]);
        qCfg.push_back(config);
      } else {
        nwCfg.push_back(connection);
      }
    }

    QueueRegistry::get().configure(qCfg);
    NetworkManager::get().configure(nwCfg);
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
  std::shared_ptr<SenderConcept<Datatype>> get_sender(Endpoint const& endpoint)
  {
    if (endpoint.direction == Direction::kInput) {
      throw ConnectionDirectionMismatch(ERS_HERE, to_string(endpoint), "input", "sender");
    }

    static std::mutex dt_sender_mutex;
    std::lock_guard<std::mutex> lk(dt_sender_mutex);

    if (!m_senders.count(endpoint)) {
      if (QueueRegistry::get().has_queue(endpoint)) { // if queue
        TLOG() << "Creating QueueSenderModel for endpoint " << to_string(endpoint);
        m_senders[endpoint] = std::make_shared<QueueSenderModel<Datatype>>(QueueSenderModel<Datatype>(endpoint));
      } else {
        TLOG() << "Creating NetworkSenderModel for endpoint " << to_string(endpoint);
        m_senders[endpoint] =
          std::make_shared<NetworkSenderModel<Datatype>>(NetworkSenderModel<Datatype>(endpoint));
      }
    }
    return std::dynamic_pointer_cast<SenderConcept<Datatype>>(m_senders[endpoint]);
  }

  template<typename Datatype>
  std::shared_ptr<ReceiverConcept<Datatype>> get_receiver(Endpoint const& endpoint)
  {
    if (endpoint.direction == Direction::kOutput) {
      throw ConnectionDirectionMismatch(ERS_HERE, to_string(endpoint), "output", "receiver");
    }

    static std::mutex dt_receiver_mutex;
    std::lock_guard<std::mutex> lk(dt_receiver_mutex);

    if (!m_receivers.count(endpoint)) {
      if (QueueRegistry::get().has_queue(endpoint)) { // if queue
        TLOG() << "Creating QueueReceiverModel for endpoint " << to_string(endpoint);
        m_receivers[endpoint] = std::make_shared<QueueReceiverModel<Datatype>>(QueueReceiverModel<Datatype>(endpoint));
      } else {
        TLOG() << "Creating NetworkReceiverModel for endpoint " << to_string(endpoint);
        m_receivers[endpoint] =
          std::make_shared<NetworkReceiverModel<Datatype>>(NetworkReceiverModel<Datatype>(endpoint));
      }
    }
    return std::dynamic_pointer_cast<ReceiverConcept<Datatype>>(m_receivers[endpoint]); // NOLINT
  }

  template<typename Datatype>
  void add_callback(Endpoint const& endpoint, std::function<void(Datatype&)> callback)
  {
    auto receiver = get_receiver<Datatype>(endpoint);
    receiver->add_callback(callback);
  }

  template<typename Datatype>
  void remove_callback(Endpoint const& endpoint)
  {
    auto receiver = get_receiver<Datatype>(endpoint);
    receiver->remove_callback();
  }

private:
  IOManager() {}

  using SenderMap = std::map<Endpoint, std::shared_ptr<Sender>>;
  using ReceiverMap = std::map<Endpoint, std::shared_ptr<Receiver>>;
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
get_iom_sender(iomanager::Endpoint const& endpoint)
{
  return iomanager::IOManager::get()->get_sender<Datatype>(endpoint);
}

template<typename Datatype>
static std::shared_ptr<iomanager::ReceiverConcept<Datatype>> // NOLINT(build/namespaces)
get_iom_receiver(iomanager::Endpoint const& endpoint)
{
  return iomanager::IOManager::get()->get_receiver<Datatype>(endpoint);
}

template<typename Datatype>
static std::shared_ptr<iomanager::SenderConcept<Datatype>> // NOLINT(build/namespaces)
get_iom_sender(std::string const& conn_uid)
{
  return iomanager::IOManager::get()->get_sender<Datatype>(conn_uid);
}

template<typename Datatype>
static std::shared_ptr<iomanager::ReceiverConcept<Datatype>> // NOLINT(build/namespaces)
get_iom_receiver(std::string const& conn_uid)
{
  return iomanager::IOManager::get()->get_receiver<Datatype>(conn_uid);
}

} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_
