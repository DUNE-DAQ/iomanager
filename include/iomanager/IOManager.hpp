/**
 * @file IOManager.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_

#include "iomanager/ConfigClient.hpp"
#include "iomanager/ConnectionId.hpp"
#include "iomanager/NetworkManager.hpp"
#include "iomanager/QueueRegistry.hpp"
#include "iomanager/Receiver.hpp"
#include "iomanager/Sender.hpp"

#include "nlohmann/json.hpp"

#include <cstdlib>
#include <map>
#include <memory>
#include <regex>
#include <string>

namespace dunedaq {
// Disable coverage collection LCOV_EXCL_START
ERS_DECLARE_ISSUE(iomanager,
                  ConnectionDirectionMismatch,
                  "Connection reference with name " << name << " specified direction " << direction
                                                    << ", but tried to obtain a " << handle_type,
                  ((std::string)name)((std::string)direction)((std::string)handle_type))
// Re-enable coverage collection LCOV_EXCL_STOP

namespace iomanager {

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

  void configure(ConnectionIds_t connections)
  {
    m_connections = connections;
    std::map<std::string, QueueConfig> qCfg;
    ConnectionIds_t nwCfg;
    std::regex queue_uri_regex("queue://(\\w+):(\\d+)");

    for (auto& connection : m_connections) {
      if (connection.service_type == ServiceType::kQueue) {
        std::smatch sm;
        std::regex_match(connection.uri, sm, queue_uri_regex);
        qCfg[connection.uid].kind = QueueConfig::stoqk(sm[1]);
        qCfg[connection.uid].capacity = stoi(sm[2]);
      } else {

        // here if connection is a host or a source name. If
        // it's a source, don't add it to the nwmgr config yet
        // Update connection.uri ahen we want to connect
        if (connection.uri.substr(0, 4) == "src:") {
          continue;
        }

        if (connection.service_type == ServiceType::kNetSender ||
            connection.service_type == ServiceType::kNetReceiver) {

          nwCfg.push_back(connection);
        } else if (connection.service_type == ServiceType::kPublisher ||
                   connection.service_type == ServiceType::kSubscriber) {
          nwCfg.push_back(connection);
        } else {
          // throw ers issue?
        }
      }
    }

    QueueRegistry::get().configure(qCfg);
    NetworkManager::get().configure(nwCfg);
  }

  void reset()
  {
    m_connections.clear();
    QueueRegistry::get().reset();
    NetworkManager::get().reset();
    m_senders.clear();
    m_receivers.clear();
    s_instance = nullptr;
  }

  template<typename Datatype>
  std::shared_ptr<SenderConcept<Datatype>> get_sender(std::string const& connection_uid)
  {
    ConnectionRef dummy_ref{ connection_uid, connection_uid, Direction::kOutput };
    return get_sender<Datatype>(dummy_ref);
  }

  template<typename Datatype>
  std::shared_ptr<SenderConcept<Datatype>> get_sender(ConnectionRef const& conn_ref)
  {
    if (conn_ref.dir == Direction::kInput) {
      throw ConnectionDirectionMismatch(ERS_HERE, conn_ref.name, "input", "sender");
    }

    static std::mutex dt_sender_mutex;
    std::lock_guard<std::mutex> lk(dt_sender_mutex);

    if (!m_senders.count(conn_ref)) {
      // create from lookup service's factory function
      // based on connID we know if it's queue or network
      auto conn_id = ref_to_id(conn_ref);
      if (conn_id.service_type == ServiceType::kQueue) { // if queue
        TLOG() << "Creating QueueSenderModel for service_name " << conn_id.uid;
        m_senders[conn_ref] =
          std::make_shared<QueueSenderModel<Datatype>>(QueueSenderModel<Datatype>(conn_id, conn_ref));
      } else if (conn_id.service_type == ServiceType::kNetSender || conn_id.service_type == ServiceType::kPublisher) {
        TLOG() << "Creating NetworkSenderModel for service_name " << conn_id.uid;

        // Check here if connection is a host or a source name. If
        // it's a source. look up the host in the config server and
        // update conn_id.uri
        if (conn_id.uri.substr(0, 4) == "src:") {
          std::string connectionServer = "configdict.connections";
          char* env = getenv("CONNECTION_SERVER");
          if (env) {
            connectionServer = std::string(env);
          }
          std::string connectionPort = "5000";
          env = getenv("CONNECTION_PORT");
          if (env) {
            connectionPort = std::string(env);
          }
          ConfigClient cc(connectionServer, connectionPort);
          int gstart = 4;
          if (conn_id.uri.substr(gstart, 2) == "//") {
            gstart += 2;
          }
          std::string app = cc.getSourceApp(conn_id.uri.substr(gstart));
          std::string conf = cc.getAppConfig(app);
          auto jsconf = nlohmann::json::parse(conf);
          std::string host = jsconf["host"];
          std::string port = jsconf["port"];
          std::cout << "Replacing conn_id.uri <" << conn_id.uri << ">";
          conn_id.uri = "tcp://" + host + ":" + port;
          std::cout << " with <" << conn_id.uri << ">" << std::endl;
          ConnectionIds_t nwCfg;
          nwCfg.push_back(conn_id);
          NetworkManager::get().configure(nwCfg);
        }

        m_senders[conn_ref] =
          std::make_shared<NetworkSenderModel<Datatype>>(NetworkSenderModel<Datatype>(conn_id, conn_ref));
      } else {
        throw ConnectionDirectionMismatch(ERS_HERE, conn_ref.name, "output", connection::str(conn_id.service_type));
      }
    }
    return std::dynamic_pointer_cast<SenderConcept<Datatype>>(m_senders[conn_ref]);
  }

  template<typename Datatype>
  std::shared_ptr<ReceiverConcept<Datatype>> get_receiver(std::string const& connection_uid)
  {
    ConnectionRef dummy_ref{ connection_uid, connection_uid, Direction::kInput };
    return get_receiver<Datatype>(dummy_ref);
  }

  template<typename Datatype>
  std::shared_ptr<ReceiverConcept<Datatype>> get_receiver(ConnectionRef const& conn_ref)
  {
    if (conn_ref.dir == Direction::kOutput) {
      throw ConnectionDirectionMismatch(ERS_HERE, conn_ref.name, "output", "receiver");
    }

    static std::mutex dt_receiver_mutex;
    std::lock_guard<std::mutex> lk(dt_receiver_mutex);

    if (!m_receivers.count(conn_ref)) {
      auto conn_id = ref_to_id(conn_ref);
      if (conn_id.service_type == ServiceType::kQueue) { // if queue
        TLOG() << "Creating QueueReceiverModel for service_name " << conn_id.uid;
        m_receivers[conn_ref] =
          std::make_shared<QueueReceiverModel<Datatype>>(QueueReceiverModel<Datatype>(conn_id, conn_ref));
      } else if (conn_id.service_type == ServiceType::kNetReceiver) {
        TLOG() << "Creating NetworkReceiverModel for service_name " << conn_id.uid;
        m_receivers[conn_ref] =
          std::make_shared<NetworkReceiverModel<Datatype>>(NetworkReceiverModel<Datatype>(conn_id, conn_ref));
      } else if (conn_id.service_type == ServiceType::kSubscriber) {
        TLOG() << "Creating NetworkReceiverModel for service_name " << conn_ref.uid;
        // This ConnectionRef refers to a topic if its uid is not the same as the returned ConnectionId's uid
        m_receivers[conn_ref] = std::make_shared<NetworkReceiverModel<Datatype>>(
          NetworkReceiverModel<Datatype>(conn_id, conn_ref, conn_id.uid != conn_ref.uid));
      } else {
        throw ConnectionDirectionMismatch(ERS_HERE, conn_ref.name, "input", connection::str(conn_id.service_type));
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

  const ConnectionId ref_to_id(ConnectionRef const& ref) const
  {
    for (auto& conn : m_connections) {
      if (conn.uid == ref.uid)
        return conn;
    }

    // Subscribers can have a UID that is a topic they are interested in. Return the first matching conn ID
    if (ref.dir == Direction::kInput) {
      for (auto& conn : m_connections) {
        if (conn.service_type == ServiceType::kSubscriber) {
          for (auto& topic : conn.topics) {
            if (topic == ref.uid)
              return conn;
          }
        }
      }
    }

    throw ConnectionNotFound(ERS_HERE, ref.uid);
  }

private:
  IOManager() {}

  using SenderMap = std::map<ConnectionRef, std::shared_ptr<Sender>>;
  using ReceiverMap = std::map<ConnectionRef, std::shared_ptr<Receiver>>;
  ConnectionIds_t m_connections;
  SenderMap m_senders;
  ReceiverMap m_receivers;

  static std::shared_ptr<IOManager> s_instance;
};

} // namespace iomanager

// Helper functions
[[maybe_unused]] static std::shared_ptr<iomanager::IOManager>
get_iomanager()
{
  return iomanager::IOManager::get();
}

template<typename Datatype>
static std::shared_ptr<iomanager::SenderConcept<Datatype>>
get_iom_sender(iomanager::ConnectionRef const& conn_ref)
{
  return iomanager::IOManager::get()->get_sender<Datatype>(conn_ref);
}

template<typename Datatype>
static std::shared_ptr<iomanager::ReceiverConcept<Datatype>>
get_iom_receiver(iomanager::ConnectionRef const& conn_ref)
{
  return iomanager::IOManager::get()->get_receiver<Datatype>(conn_ref);
}

template<typename Datatype>
static std::shared_ptr<iomanager::SenderConcept<Datatype>>
get_iom_sender(std::string const& conn_uid)
{
  return iomanager::IOManager::get()->get_sender<Datatype>(conn_uid);
}

template<typename Datatype>
static std::shared_ptr<iomanager::ReceiverConcept<Datatype>>
get_iom_receiver(std::string const& conn_uid)
{
  return iomanager::IOManager::get()->get_receiver<Datatype>(conn_uid);
}

} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_
