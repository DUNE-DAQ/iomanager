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
    ci.add(to_string(sender.first), tmp_ic);
  }

  for (auto& receiver : m_receiver_plugins) {
    opmonlib::InfoCollector tmp_ic;
    receiver.second->get_info(tmp_ic, level);
    ci.add(to_string(receiver.first), tmp_ic);
  }
}

void
NetworkManager::configure(const Connections_t& connections)
{
  if (!m_preconfigured_connections.empty()) {
    throw AlreadyConfigured(ERS_HERE);
  }

  for (auto& connection : connections) {
    auto name = connection_name(connection);
    TLOG_DEBUG(15) << "Adding connection " << name << " to connection map";
    if (m_preconfigured_connections.count(name)) {
      TLOG_DEBUG(15) << "Name collision for connection " << name
                     << " connection_map.count: " << m_preconfigured_connections.count(name);
      reset();
      throw NameCollision(ERS_HERE, connection.uri);
    }
    m_preconfigured_connections[name] = connection;

    if (m_config_client == nullptr) {

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
      m_config_client = std::make_unique<ConfigClient>(connectionServer, connectionPort);
    }
  }
}

void
NetworkManager::reset()
{
  {
    std::lock_guard<std::mutex> lk(m_sender_plugin_map_mutex);
    m_sender_plugins.clear();
  }
  {
    std::lock_guard<std::mutex> lk(m_receiver_plugin_map_mutex);
    m_receiver_plugins.clear();
  }
  m_preconfigured_connections.clear();
}

std::shared_ptr<ipm::Receiver>
NetworkManager::get_receiver(ConnectionRequest const& request)
{
  TLOG_DEBUG(9) << "Getting receiver for request " << to_string(request);

  std::lock_guard<std::mutex> lk(m_receiver_plugin_map_mutex);
  if (!m_receiver_plugins.count(request) || m_receiver_plugins.at(request) == nullptr) {
    auto response = get_preconfigured_connections(request);
    if (response.uris.size() == 0) {
      response = m_config_client->resolveEndpoint(request);
    }

    if (response.uris.size() == 0) {
      throw ConnectionNotFound(ERS_HERE, to_string(request));
    }

    TLOG_DEBUG(9) << "Creating receiver for request " << to_string(request);
    auto receiver = create_receiver(response, request.data_type);

    m_receiver_plugins[request] = receiver;
  }

  return m_receiver_plugins[request];
}

std::shared_ptr<ipm::Sender>
NetworkManager::get_sender(ConnectionRequest const& request)
{
  TLOG_DEBUG(10) << "Getting sender for request " << to_string(request);

  std::lock_guard<std::mutex> lk(m_sender_plugin_map_mutex);

  if (!m_sender_plugins.count(request) || m_sender_plugins.at(request) == nullptr) {

    auto response = get_preconfigured_connections(request);
    if (response.uris.size() > 1) {
      throw NameCollision(ERS_HERE, to_string(request));
    }
    if (response.uris.size() == 0) {
      response = m_config_client->resolveEndpoint(request);
      if (response.uris.size() == 0) {
        throw ConnectionNotFound(ERS_HERE, to_string(request));
      }
      if (response.uris.size() > 1) {
        throw NameCollision(ERS_HERE, to_string(request));
      }
    }

    TLOG_DEBUG(10) << "Creating sender for request " << to_string(request);
    auto sender = create_sender(response);
    m_sender_plugins[request] = sender;
  }
  return m_sender_plugins[request];
}

bool
NetworkManager::is_pubsub_connection(ConnectionRequest const& request) const
{
  auto connections = get_preconfigured_connections(request);
  if (connections.uris.size() == 0) {
    connections = m_config_client->resolveEndpoint(request);
  }
  if (connections.uris.size() == 0) {
    throw ConnectionNotFound(ERS_HERE, to_string(request));
  }
  bool is_pubsub = connections.connection_type == ConnectionType::kPubSub;

  // TLOG() << "Returning " << std::boolalpha << is_pubsub << " for request " << to_string(request);
  return is_pubsub;
}

std::string
NetworkManager::GetUriForConnection(Connection conn)
{

  // Check here if connection is a host or a source name. If
  // it's a source. look up the host in the config server and
  // update conn.uri
  if (conn.uri.substr(0, 4) == "src:") {
    int gstart = 4;
    if (conn.uri.substr(gstart, 2) == "//") {
      gstart += 2;
    }

    std::string conf = m_config_client->resolveConnection(conn);
    auto jsconf = nlohmann::json::parse(conf);
    TLOG() << "Replacing conn.uri <" << conn.uri << ">"
           << " with <" << jsconf["uri"] << ">";
    conn.uri = jsconf["uri"];
  }

  return conn.uri;
}

ConnectionResponse
NetworkManager::get_preconfigured_connections(ConnectionRequest const& request) const
{
  ConnectionResponse matching_connections;
  for (auto& conn : m_preconfigured_connections) {
    if (is_match(request, conn.second)) {
      matching_connections.connection_type = conn.second.connection_type;
      matching_connections.uris.push_back(conn.second.uri);
    }
  }

  return matching_connections;
}

std::shared_ptr<ipm::Receiver>
NetworkManager::create_receiver(ConnectionResponse response, std::string const& data_type)
{
  TLOG_DEBUG(12) << "START";
  if (response.uris.size() == 0) {
    return nullptr;
  }

  bool is_pubsub = response.connection_type == ConnectionType::kPubSub;
  if (response.uris.size() > 1 && !is_pubsub) {
    throw OperationFailed(ERS_HERE,
                          "Trying to configure a kSendRecv receiver with multiple Connections is not allowed!");
  }

  auto plugin_type =
    ipm::get_recommended_plugin_name(is_pubsub ? ipm::IpmPluginType::Subscriber : ipm::IpmPluginType::Receiver);

  TLOG_DEBUG(12) << "Creating plugin of type " << plugin_type;
  auto plugin = dunedaq::ipm::make_ipm_receiver(plugin_type);

  nlohmann::json config_json;
  if (is_pubsub) {
    config_json["connection_strings"] = response.uris;
  } else {
    config_json["connection_string"] = response.uris[0];
  }
  plugin->connect_for_receives(config_json);

  if (is_pubsub) {
    TLOG_DEBUG(12) << "Subscribing to topic " << data_type << " after connect_for_receives";
    auto subscriber = std::dynamic_pointer_cast<ipm::Subscriber>(plugin);
    subscriber->subscribe(data_type);
  }

  TLOG_DEBUG(12) << "END";
  return plugin;
}

std::shared_ptr<ipm::Sender>
NetworkManager::create_sender(ConnectionResponse connection)
{
  auto is_pubsub = connection.connection_type == ConnectionType::kPubSub;
  auto plugin_type =
    ipm::get_recommended_plugin_name(is_pubsub ? ipm::IpmPluginType::Publisher : ipm::IpmPluginType::Sender);

  TLOG_DEBUG(11) << "Creating sender plugin of type " << plugin_type;
  auto plugin = dunedaq::ipm::make_ipm_sender(plugin_type);
  TLOG_DEBUG(11) << "Connecting sender plugin";
  plugin->connect_for_sends({ { "connection_string", connection.uris[0] } });

  return plugin;
}

} // namespace dunedaq::iomanager
