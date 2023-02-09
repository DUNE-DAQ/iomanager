/**
 * @file NetworkManager.hpp NetworkManager Class implementations
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/network/NetworkManager.hpp"
#include "iomanager/SchemaUtils.hpp"

#include "iomanager/connectioninfo/InfoNljs.hpp"

#include "ipm/PluginInfo.hpp"
#include "logging/Logging.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

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
NetworkManager::gather_stats(opmonlib::InfoCollector& ci, int level)
{

  for (auto& sender : m_sender_plugins) {
    opmonlib::InfoCollector tmp_ic;
    sender.second->get_info(tmp_ic, level);
    ci.add(sender.first.uid, tmp_ic);
  }

  for (auto& receiver : m_receiver_plugins) {
    opmonlib::InfoCollector tmp_ic;
    receiver.second->get_info(tmp_ic, level);
    ci.add(receiver.first.uid, tmp_ic);
  }
}

void
NetworkManager::configure(const Connections_t& connections, bool use_config_client, std::chrono::milliseconds config_client_interval)
{
  if (!m_preconfigured_connections.empty()) {
    throw AlreadyConfigured(ERS_HERE);
  }

  for (auto& connection : connections) {
    auto name = connection.id.uid;
    TLOG_DEBUG(15) << "Adding connection " << name << " to connection map";
    if (m_preconfigured_connections.count(connection.id)) {
      TLOG_DEBUG(15) << "Name collision for connection " << name << ", DT " << connection.id.data_type
                     << " connection_map.count: " << m_preconfigured_connections.count(connection.id);
      reset();
      throw NameCollision(ERS_HERE, connection.id.uid);
    }
    m_preconfigured_connections[connection.id] = connection;
  }

  if (use_config_client && m_config_client == nullptr) {

    std::string connectionServer = "localhost";
    char* env = getenv("CONNECTION_SERVER");
    if (env) {
      connectionServer = std::string(env);
    }
    std::string connectionPort = "5000";
    env = getenv("CONNECTION_PORT");
    if (env) {
      connectionPort = std::string(env);
    }
    m_config_client = std::make_unique<ConfigClient>(connectionServer, connectionPort, config_client_interval);
    m_config_client_interval = config_client_interval;
  }
}

void
NetworkManager::reset()
{
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
    m_config_client->retract();
  }
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
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count() <
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
    if (is_match(conn_id, conn.second.id)) {
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
    if (conn.second.id.uid == uid)
      output.insert(conn.second.id.data_type);
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
    for (auto& conn : connections)
      uris.push_back(conn.uri);
    config_json["connection_strings"] = uris;
  } else {
    config_json["connection_string"] = connections[0].uri;
  }
  connections[0].uri = plugin->connect_for_receives(config_json);
  TLOG_DEBUG(12) << "Receiver reports connected to URI " << connections[0].uri;

  if (is_pubsub) {
    TLOG_DEBUG(12) << "Subscribing to topic " << connections[0].data_type << " after connect_for_receives";
    auto subscriber = std::dynamic_pointer_cast<ipm::Subscriber>(plugin);
    subscriber->subscribe(connections[0].data_type);
    std::lock_guard<std::mutex> lkk(m_subscriber_plugin_map_mutex);
    m_subscriber_plugins[conn_id] = subscriber;
    if (!m_subscriber_update_thread_running) {
      m_subscriber_update_thread_running = true;
      m_subscriber_update_thread.reset(new std::thread(&NetworkManager::update_subscribers, this));
    }
  }

  if (m_config_client != nullptr && !is_pubsub) {
    m_config_client->publish(connections[0]);
  }

  TLOG_DEBUG(12) << "END";
  return plugin;
}

std::shared_ptr<ipm::Sender>
NetworkManager::create_sender(ConnectionInfo connection)
{
  auto is_pubsub = connection.connection_type == ConnectionType::kPubSub;
  auto plugin_type =
    ipm::get_recommended_plugin_name(is_pubsub ? ipm::IpmPluginType::Publisher : ipm::IpmPluginType::Sender);

  TLOG_DEBUG(11) << "Creating sender plugin of type " << plugin_type;
  auto plugin = dunedaq::ipm::make_ipm_sender(plugin_type);
  TLOG_DEBUG(11) << "Connecting sender plugin to " << connection.uri;
  connection.uri = plugin->connect_for_sends({ { "connection_string", connection.uri } });
  TLOG_DEBUG(11) << "Sender Plugin connected, reports URI " << connection.uri;

  if (m_config_client != nullptr && is_pubsub) {
    m_config_client->publish(connection);
  }

  return plugin;
}
void
NetworkManager::update_subscribers()
{
  while (m_subscriber_update_thread_running.load()) {
    {
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

} // namespace dunedaq::iomanager
