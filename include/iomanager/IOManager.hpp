/**
 * @file IOManager.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_

#include "iomanager/ConnectionId.hpp"
#include "iomanager/QueueRegistry.hpp"
#include "iomanager/Receiver.hpp"
#include "iomanager/Sender.hpp"
#include "networkmanager/NetworkManager.hpp"

#include "nlohmann/json.hpp"

#include <map>
#include <memory>
#include <regex>
#include <string>

namespace dunedaq {
ERS_DECLARE_ISSUE(iomanager,
                  ConnectionNotFound,
                  "Connection with uid " << conn_uid << " not found!",
                  ((std::string)conn_uid))

namespace iomanager {

/**
 * @class IOManager
 * Wrapper class for sockets and SPSC circular buffers.
 *   Makes the communication between DAQ processes easier and scalable.
 */
class IOManager
{

public:
  IOManager() {}

  IOManager(const IOManager&) = delete;            ///< IOManager is not copy-constructible
  IOManager& operator=(const IOManager&) = delete; ///< IOManager is not copy-assignable
  IOManager(IOManager&&) = delete;                 ///< IOManager is not move-constructible
  IOManager& operator=(IOManager&&) = delete;      ///< IOManager is not move-assignable

  static void configure(ConnectionIds_t connections)
  {
    s_connections = connections;
    std::map<std::string, QueueConfig> qCfg;
    dunedaq::networkmanager::nwmgr::Connections nwCfg;
    std::regex queue_uri_regex("queue://(\\w+):(\\d+)");
    for (auto& connection : s_connections) {
      if (connection.service_type == ServiceType::kQueue) {
        std::smatch sm;
        std::regex_match(connection.uri, sm, queue_uri_regex);
        qCfg[connection.uid].kind = QueueConfig::stoqk(sm[1]);
        qCfg[connection.uid].capacity = stoi(sm[2]);
      } else if (connection.service_type == ServiceType::kNetwork) {
        dunedaq::networkmanager::nwmgr::Connection this_conn;
        this_conn.name = connection.uid;
        this_conn.address = connection.uri;
        nwCfg.push_back(this_conn);
      } else {
        // throw ers issue?
      }
    }

    QueueRegistry::get().configure(qCfg);
    networkmanager::NetworkManager::get().configure(nwCfg);
  }

  static void reset()
  {
    s_connections.clear();
    QueueRegistry::get().reset();
    networkmanager::NetworkManager::get().reset();
  }

  template<typename Datatype>
  std::shared_ptr<SenderConcept<Datatype>> get_sender(ConnectionRef const& conn_ref)
  {
    if (!m_senders.count(conn_ref)) {
      // create from lookup service's factory function
      // based on connID we know if it's queue or network
      auto conn_id = ref_to_id(conn_ref);
      if (conn_id.service_type == ServiceType::kQueue) { // if queue
        TLOG() << "Creating QueueSenderModel for service_name " << conn_id.uid;
        m_senders[conn_ref] =
          std::make_shared<QueueSenderModel<Datatype>>(QueueSenderModel<Datatype>(conn_id, conn_ref));
      } else {
        TLOG() << "Creating NetworkSenderModel for service_name " << conn_id.uid;
        m_senders[conn_ref] =
          std::make_shared<NetworkSenderModel<Datatype>>(NetworkSenderModel<Datatype>(conn_id, conn_ref));
      }
    }
    return std::dynamic_pointer_cast<SenderConcept<Datatype>>(m_senders[conn_ref]);
  }

  template<typename Datatype>
  std::shared_ptr<ReceiverConcept<Datatype>> get_receiver(ConnectionRef const& conn_ref)
  {
    if (!m_receivers.count(conn_ref)) {
      auto conn_id = ref_to_id(conn_ref);
      if (conn_id.service_type == ServiceType::kQueue) { // if queue
        TLOG() << "Creating QueueReceiverModel for service_name " << conn_id.uid;
        m_receivers[conn_ref] =
          std::make_shared<QueueReceiverModel<Datatype>>(QueueReceiverModel<Datatype>(conn_id, conn_ref));
      } else {
        TLOG() << "Creating NetworkReceiverModel for service_name " << conn_id.uid;
        m_receivers[conn_ref] =
          std::make_shared<NetworkReceiverModel<Datatype>>(NetworkReceiverModel<Datatype>(conn_id, conn_ref));
      }
    }
    return std::dynamic_pointer_cast<ReceiverConcept<Datatype>>(m_receivers[conn_ref]); // NOLINT
  }

  template<typename Datatype>
  void add_callback(ConnectionRef const& conn_ref, std::function<void(Datatype&)> callback)
  {
    auto receiver = get_receiver<Datatype>(conn_ref);
    receiver->add_callback(callback);
  }

  template<typename Datatype>
  void remove_callback(ConnectionRef const& conn_ref)
  {
    auto receiver = get_receiver<Datatype>(conn_ref);
    receiver->remove_callback();
  }

private:
  static ConnectionId ref_to_id(ConnectionRef const& ref)
  {
    for (auto& conn : s_connections) {
      if (conn.uid == ref.uid)
        return conn;
    }

    throw ConnectionNotFound(ERS_HERE, ref.uid);
  }

  using SenderMap = std::map<ConnectionRef, std::shared_ptr<Sender>>;
  using ReceiverMap = std::map<ConnectionRef, std::shared_ptr<Receiver>>;
  static ConnectionIds_t s_connections;
  SenderMap m_senders;
  ReceiverMap m_receivers;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_
