/**
 * @file NetworkManager.hpp NetworkManager Class implementations
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/network/NetworkManager.hpp"
#include "iomanager/SchemaUtils.hpp"

#include "ipm/PluginInfo.hpp"
#include "logging/Logging.hpp"
#include "utilities/Resolver.hpp"

#include "confmodel/PhysicalHost.hpp"
#include "confmodel/Service.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <fmt/format.h>

namespace dunedaq::iomanager {

std::unique_ptr<NetworkManager> NetworkManager::s_instance = nullptr;

NetworkManager&
NetworkManager::get()
{
  if (!s_instance) {
    s_instance.reset(new NetworkManager());
  }
  return *s_instance;
}

void
NetworkManager::configure(const std::string& session_name,
                          const std::vector<const confmodel::NetworkConnection*>& connections,
                          const confmodel::ConnectivityService* conn_svc,
                          dunedaq::opmonlib::OpMonManager& opmgr)
{
  if (!m_preconfigured_connections.empty()) {
    throw AlreadyConfigured(ERS_HERE);
  }

  for (auto& connection : connections) {
    auto name = connection->UID();
    TLOG_DEBUG(15) << "Adding connection " << name << " to connection map";
    ConnectionId id(connection);
    if (m_preconfigured_connections.count(id)) {
      TLOG_DEBUG(15) << "Name collision for connection " << name << ", DT " << connection->get_data_type()
                     << " connection_map.count: " << m_preconfigured_connections.count(id);
      reset();
      throw NameCollision(ERS_HERE, connection->UID());
    }
    m_preconfigured_connections[id] = connection;
  }

  if (conn_svc != nullptr) {

    auto service = conn_svc->get_service();

    auto connectionServer = conn_svc->get_host();
    auto connectionPort = service->get_port();
    auto config_client_interval = std::chrono::milliseconds(conn_svc->get_interval_ms());

    TLOG_DEBUG(17) << "ConnectionServer host and port are " << connectionServer << ":" << connectionPort;
    if (m_config_client == nullptr) {
      m_config_client =
        std::make_unique<ConfigClient>(connectionServer, std::to_string(connectionPort),session_name, config_client_interval);
    }
    m_config_client_interval = config_client_interval;
  }

  opmgr.register_node("senders", m_sender_opmon_link);
  opmgr.register_node("receivers", m_receiver_opmon_link);
}

void
NetworkManager::reset()
{
  TLOG_DEBUG(5) << "reset() BEGIN";
  m_subscriber_update_thread_running = false;
  if (m_subscriber_update_thread && m_subscriber_update_thread->joinable()) {
    m_subscriber_update_thread->join();
  }
  {
    std::lock_guard<std::mutex> lkk(m_subscriber_plugin_map_mutex);
    m_subscriber_plugins.clear();
  }
  {
    std::lock_guard<std::mutex> lk(m_sender_plugin_map_mutex);
    m_sender_plugins.clear();
  }
  {
    std::lock_guard<std::mutex> lk(m_receiver_plugin_map_mutex);
    m_receiver_plugins.clear();
  }

  m_preconfigured_connections.clear();
  if (m_config_client != nullptr) {
    try {
      m_config_client->retract();
    } catch (FailedRetract const& r) {
      ers::error(r);
    }
  }
  m_config_client.reset(nullptr);

  m_sender_opmon_link = std::make_shared<dunedaq::opmonlib::OpMonLink>();
  m_receiver_opmon_link = std::make_shared<dunedaq::opmonlib::OpMonLink>();
  TLOG_DEBUG(5) << "reset() END";
}

void
NetworkManager::shutdown()
{
  TLOG_DEBUG(5) << "shutdown() BEGIN";
  m_subscriber_update_thread_running = false;
  if (m_subscriber_update_thread && m_subscriber_update_thread->joinable()) {
    m_subscriber_update_thread->join();
  }
  {
    std::lock_guard<std::mutex> lkk(m_subscriber_plugin_map_mutex);
    m_subscriber_plugins.clear();
  }
  {
    std::lock_guard<std::mutex> lk(m_sender_plugin_map_mutex);
    m_sender_plugins.clear();
  }
  {
    std::lock_guard<std::mutex> lk(m_receiver_plugin_map_mutex);
    m_receiver_plugins.clear();
  }

  if (m_config_client != nullptr) {
    try {
      m_config_client->retract();
    } catch (FailedRetract const& r) {
      ers::error(r);
    }
  }
  TLOG_DEBUG(5) << "shutdown() END";
}

std::shared_ptr<ipm::Receiver>
NetworkManager::get_receiver(ConnectionId const& conn_id)
{
  TLOG_DEBUG(9) << "Getting receiver for connection " << conn_id.uid;

  std::lock_guard<std::mutex> lk(m_receiver_plugin_map_mutex);
  if (!m_receiver_plugins.count(conn_id) || m_receiver_plugins.at(conn_id) == nullptr) {

    auto response = get_connections(conn_id);

    TLOG_DEBUG(9) << "Creating receiver for connection " << conn_id.uid;
    auto receiver = create_receiver(response.connections, conn_id);

    m_receiver_plugins[conn_id] = receiver;
  }

  return m_receiver_plugins[conn_id];
}

std::shared_ptr<ipm::Sender>
NetworkManager::get_sender(ConnectionId const& conn_id)
{
  TLOG_DEBUG(10) << "Getting sender for connection " << conn_id.uid;

  std::lock_guard<std::mutex> lk(m_sender_plugin_map_mutex);

  if (!m_sender_plugins.count(conn_id) || m_sender_plugins.at(conn_id) == nullptr) {
    auto response = get_connections(conn_id, true);

    TLOG_DEBUG(10) << "Creating sender for connection " << conn_id.uid;
    auto sender = create_sender(response.connections[0]);
    m_sender_plugins[conn_id] = sender;
  }

  if (m_sender_plugins.count(conn_id)) {
    return m_sender_plugins[conn_id];
  }

  return nullptr;
}

void
NetworkManager::remove_sender(ConnectionId const& conn_id)
{
  TLOG_DEBUG(10) << "Removing sender for connection " << conn_id.uid;

  std::lock_guard<std::mutex> lk(m_sender_plugin_map_mutex);
  m_sender_plugins.erase(conn_id);
}


bool
NetworkManager::is_pubsub_connection(ConnectionId const& conn_id) const
{
  auto response = get_connections(conn_id);
  bool is_pubsub = response.connections[0].connection_type == ConnectionType::kPubSub;

  // TLOG() << "Returning " << std::boolalpha << is_pubsub << " for request " << request;
  return is_pubsub;
}

ConnectionResponse
NetworkManager::get_connections(ConnectionId const& conn_id, bool restrict_single) const
{
  auto response = get_preconfigured_connections(conn_id);
  if (restrict_single && response.connections.size() > 1) {
    throw NameCollision(ERS_HERE, conn_id.uid);
  }
  if (m_config_client != nullptr) {
    auto start_time = std::chrono::steady_clock::now();
    while (
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count() <
      1000) {
      try {
        auto client_response = m_config_client->resolveConnection(conn_id, conn_id.session);
        if (restrict_single && client_response.connections.size() > 1) {
          throw NameCollision(ERS_HERE, conn_id.uid);
        }

        if (client_response.connections.size() > 0) {
          response = client_response;
        }
        break;
      } catch (FailedLookup const& lf) {
        if (m_config_client->is_connected()) {
          throw ConnectionNotFound(ERS_HERE, conn_id.uid, conn_id.data_type, lf);
        }
        usleep(1000);
      }
    }
  }

  if (response.connections.size() == 0) {
    throw ConnectionNotFound(ERS_HERE, conn_id.uid, conn_id.data_type);
  }

  return response;
}

ConnectionResponse
NetworkManager::get_preconfigured_connections(ConnectionId const& conn_id) const
{
  ConnectionResponse matching_connections;
  for (auto& conn : m_preconfigured_connections) {
    if (is_match(conn_id, conn.first)) {
      matching_connections.connections.push_back(conn.second);
    }
  }

  return matching_connections;
}

std::set<std::string>
NetworkManager::get_datatypes(std::string const& uid) const
{
  std::set<std::string> output;
  for (auto& conn : m_preconfigured_connections) {
    if (conn.second->UID() == uid)
      output.insert(conn.second->get_data_type());
  }

  return output;
}

std::shared_ptr<ipm::Receiver>
NetworkManager::create_receiver(std::vector<ConnectionInfo> connections, ConnectionId const& conn_id)
{
  TLOG_DEBUG(12) << "START";
  if (connections.size() == 0) {
    return nullptr;
  }

  bool is_pubsub = connections[0].connection_type == ConnectionType::kPubSub;
  if (connections.size() > 1 && !is_pubsub) {
    throw OperationFailed(ERS_HERE,
                          "Trying to configure a kSendRecv receiver with multiple Connections is not allowed!");
  }

  auto plugin_type =
    ipm::get_recommended_plugin_name(is_pubsub ? ipm::IpmPluginType::Subscriber : ipm::IpmPluginType::Receiver);

  TLOG_DEBUG(12) << "Creating plugin of type " << plugin_type;
  auto plugin = dunedaq::ipm::make_ipm_receiver(plugin_type);

  nlohmann::json config_json;
  if (is_pubsub) {
    std::vector<std::string> uris;
    for (auto& conn : connections) {
      // Check for case where both ends are in app and ConnectivityService hasn't received other end yet
      if (conn.uri.find("*") != std::string::npos || conn.uri.find("0.0.0.0") != std::string::npos) {
        continue;
      }
      uris.push_back(conn.uri);
    }
    if (uris.size() == 0) {
      return nullptr;
    }
    config_json["connection_strings"] = uris;
  } else {
    config_json["connection_string"] = connections[0].uri;
  }
  auto newCs = plugin->connect_for_receives(config_json);
  TLOG_DEBUG(12) << "Receiver reports connected to URI " << newCs;

  // Replace with resolved if there are wildcards (host and/or port)
  if (connections[0].uri.find("*") != std::string::npos || connections[0].uri.find("0.0.0.0") != std::string::npos) {
    TLOG_DEBUG(14) << "Wildcard found in connection URI " << connections[0].uri << ", adjusting before publish";
    auto newUri = utilities::parse_connection_string(newCs);
    auto oldUri = utilities::parse_connection_string(connections[0].uri);

    if (oldUri.port == "*")
      oldUri.port = newUri.port;
    if (oldUri.host == "*" || oldUri.host == "0.0.0.0")
      oldUri.host = newUri.host;

    connections[0].uri = oldUri.to_string();
    TLOG_DEBUG(14) << "Connection URI is now " << connections[0].uri;
  }

  if (is_pubsub) {
    TLOG_DEBUG(12) << "Subscribing to topic " << connections[0].data_type << " after connect_for_receives";
    auto subscriber = std::dynamic_pointer_cast<ipm::Subscriber>(plugin);
    subscriber->subscribe(connections[0].data_type);
    std::lock_guard<std::mutex> lkk(m_subscriber_plugin_map_mutex);
    m_subscriber_plugins[conn_id] = subscriber;
    if (!m_subscriber_update_thread_running && m_config_client != nullptr) {
      m_subscriber_update_thread_running = true;
      m_subscriber_update_thread.reset(new std::thread(&NetworkManager::update_subscribers, this));
    }
  }

  if (m_config_client != nullptr && !is_pubsub) {
    m_config_client->publish(connections[0]);
  }

  register_monitorable_node(plugin, m_receiver_opmon_link, conn_id.uid, is_pubsub);

  TLOG_DEBUG(12) << "END";
  return plugin;
}

std::shared_ptr<ipm::Sender>
NetworkManager::create_sender(ConnectionInfo connection)
{
  auto is_pubsub = connection.connection_type == ConnectionType::kPubSub;
  auto plugin_type =
    ipm::get_recommended_plugin_name(is_pubsub ? ipm::IpmPluginType::Publisher : ipm::IpmPluginType::Sender);

  // Check for case where both ends are in app and ConnectivityService hasn't received other end yet
  if (!is_pubsub &&
      (connection.uri.find("*") != std::string::npos || connection.uri.find("0.0.0.0") != std::string::npos)) {
    return nullptr;
  }

  TLOG_DEBUG(11) << "Creating sender plugin of type " << plugin_type;
  auto plugin = dunedaq::ipm::make_ipm_sender(plugin_type);
  TLOG_DEBUG(11) << "Connecting sender plugin to " << connection.uri;
  auto newCs = plugin->connect_for_sends({ { "connection_string", connection.uri } });
  TLOG_DEBUG(11) << "Sender Plugin connected, reports URI " << newCs;

  // Replace with resolved if there are wildcards (host and/or port)
  if (connection.uri.find("*") != std::string::npos || connection.uri.find("0.0.0.0") != std::string::npos) {
    TLOG_DEBUG(13) << "Wildcard found in connection URI " << connection.uri << ", adjusting before publish";
    auto newUri = utilities::parse_connection_string(newCs);
    auto oldUri = utilities::parse_connection_string(connection.uri);

    if (oldUri.port == "*")
      oldUri.port = newUri.port;
    if (oldUri.host == "*" || oldUri.host == "0.0.0.0")
      oldUri.host = newUri.host;

    connection.uri = oldUri.to_string();
    TLOG_DEBUG(13) << "Connection URI is now " << connection.uri;
  }

  if (m_config_client != nullptr && is_pubsub) {
    m_config_client->publish(connection);
  }

  register_monitorable_node(plugin, m_sender_opmon_link, connection.uid, is_pubsub);

  return plugin;
}

void
NetworkManager::update_subscribers()
{
  while (m_subscriber_update_thread_running.load()) {
    {
      TLOG_DEBUG(14) << "Updating registered subscribers";
      std::lock_guard<std::mutex> lk(m_subscriber_plugin_map_mutex);
      for (auto& subscriber_pair : m_subscriber_plugins) {
        try {
          auto response = get_connections(subscriber_pair.first, false);

          nlohmann::json config_json;
          std::vector<std::string> uris;
          for (auto& conn : response.connections)
            uris.push_back(conn.uri);
          config_json["connection_strings"] = uris;

          subscriber_pair.second->connect_for_receives(config_json);
        } catch (ers::Issue&) {
        }
      }
    }
    std::this_thread::sleep_for(m_config_client_interval);
  }
}

void
NetworkManager::register_monitorable_node(std::shared_ptr<opmonlib::MonitorableObject> conn,
                                          std::shared_ptr<opmonlib::OpMonLink> link,
                                          const std::string& name,
                                          bool /*is_pubsub*/)
{

  try {
    link->register_node(name, conn);
  } catch (const opmonlib::NonUniqueNodeName& err) {
    bool success = false;
    size_t counter = 1;
    do {
      auto fname = fmt::format("{}--{}", name, counter);
      try {
        link->register_node(fname, conn);
        success = true;
      } catch (const opmonlib::NonUniqueNodeName& err) {
        ++counter;
      }
    } while (!success);
  }
}

} // namespace dunedaq::iomanager
