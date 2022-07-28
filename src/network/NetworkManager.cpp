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
NetworkManager::get_receiver(Endpoint const& endpoint)
{
  TLOG_DEBUG(9) << "Getting receiver for endpoint " << to_string(endpoint);

  if (endpoint.direction != Direction::kInput) {
    TLOG_DEBUG(9) << "Endpoint " << to_string(endpoint) << " is not an input!";
    throw ConnectionDirectionMismatch(ERS_HERE, to_string(endpoint), str(endpoint.direction), "receiver");
  }

  std::lock_guard<std::mutex> lk(m_receiver_plugin_map_mutex);
  if (!m_receiver_plugins.count(endpoint) || m_receiver_plugins.at(endpoint) == nullptr) {
    auto connections = get_preconfigured_connections(endpoint);
    if (connections.size() == 0) {
      connections = m_config_client->resolveEndpoint(endpoint);
    }

    if (connections.size() == 0) {
      throw ConnectionNotFound(ERS_HERE, to_string(endpoint));
    }

    TLOG_DEBUG(9) << "Creating receiver for endpoint " << to_string(endpoint);
    auto receiver = create_receiver(connections);

    m_receiver_plugins[endpoint] = receiver;
  }

  return m_receiver_plugins[endpoint];
}

std::shared_ptr<ipm::Sender>
NetworkManager::get_sender(Endpoint const& endpoint)
{
  TLOG_DEBUG(10) << "Getting sender for endpoint " << to_string(endpoint);

  if (endpoint.direction != Direction::kOutput) {
    TLOG_DEBUG(10) << "Endpoint " << to_string(endpoint) << " is not an output!";
    throw ConnectionDirectionMismatch(ERS_HERE, to_string(endpoint), str(endpoint.direction), "sender");
  }

  std::lock_guard<std::mutex> lk(m_sender_plugin_map_mutex);

  if (!m_sender_plugins.count(endpoint) || m_sender_plugins.at(endpoint) == nullptr) {

    auto preconfigured_conns = get_preconfigured_connections(endpoint);
    Connection this_conn;
    if (preconfigured_conns.size() > 0) {
      if (preconfigured_conns.size() > 1) {
        throw NameCollision(ERS_HERE, to_string(endpoint));
      }
      this_conn = preconfigured_conns[0];
    } else {
      auto resolved_conns = m_config_client->resolveEndpoint(endpoint);
      if (resolved_conns.size() == 0) {
        throw ConnectionNotFound(ERS_HERE, to_string(endpoint));
      }
      if (resolved_conns.size() > 1) {
        throw NameCollision(ERS_HERE, to_string(endpoint));
      }
      this_conn = resolved_conns[0];
    }

    TLOG_DEBUG(10) << "Creating sender for endpoint " << to_string(endpoint);
    auto sender = create_sender(this_conn);
    m_sender_plugins[endpoint] = sender;
  }
  return m_sender_plugins[endpoint];
}

bool
NetworkManager::is_pubsub_connection(Endpoint const& endpoint) const
{
  auto connections = get_preconfigured_connections(endpoint);
  if (connections.size() == 0) {
    connections = m_config_client->resolveEndpoint(endpoint);
  }
  if (connections.size() == 0) {
    throw ConnectionNotFound(ERS_HERE, to_string(endpoint));
  }
  bool is_pubsub = connections[0].connection_type == ConnectionType::kPubSub;

  //TLOG() << "Returning " << std::boolalpha << is_pubsub << " for endpoint " << to_string(endpoint);
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
    std::string app = m_config_client->getSourceApp(conn.uri.substr(gstart));
    std::string conf = m_config_client->getAppConfig(app);
    auto jsconf = nlohmann::json::parse(conf);
    std::string host = jsconf["host"];
    std::string port = jsconf["port"];
    TLOG() << "Replacing conn.uri <" << conn.uri << ">"
           << " with <tcp://" << host << ":" << port << ">";
    conn.uri = "tcp://" + host + ":" + port;
  }

  return conn.uri;
}

std::vector<Connection>
NetworkManager::get_preconfigured_connections(Endpoint const& endpoint) const
{
  std::vector<Connection> matching_connections;
  for (auto& conn : m_preconfigured_connections) {
    if (is_match(endpoint, conn.second)) {
      matching_connections.push_back(conn.second);
    }
  }

  return matching_connections;
}

std::shared_ptr<ipm::Receiver>
NetworkManager::create_receiver(std::vector<Connection> connections)
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

  TLOG_DEBUG(12) << "Creating plugin of type "
                 << plugin_type << " for connection(s) " << connection_names(connections);
  auto plugin = dunedaq::ipm::make_ipm_receiver(plugin_type);

  nlohmann::json config_json;
  if (is_pubsub) {
    std::vector<std::string> connection_strings;
    for (auto& conn : connections) {
      connection_strings.push_back(conn.uri);
    }
    config_json["connection_strings"] = connection_strings;
  } else {
    config_json["connection_string"] = connections[0].uri;
  }
  plugin->connect_for_receives(config_json);

  if (connections[0].connection_type == ConnectionType::kPubSub) {
    TLOG_DEBUG(12) << "Subscribing to topic " << connections[0].bind_endpoint.data_type
                   << " after connect_for_receives";
    auto subscriber = std::dynamic_pointer_cast<ipm::Subscriber>(plugin);
    subscriber->subscribe(connections[0].bind_endpoint.data_type);
  }

  TLOG_DEBUG(12) << "END";
  return plugin;
}

std::shared_ptr<ipm::Sender>
NetworkManager::create_sender(Connection connection)
{
  auto is_pubsub = connection.connection_type == ConnectionType::kPubSub;
  auto plugin_type =
    ipm::get_recommended_plugin_name(is_pubsub ? ipm::IpmPluginType::Publisher : ipm::IpmPluginType::Sender);

  TLOG_DEBUG(11) << "Creating sender plugin for connection " << connection_name(connection) << " of type " << plugin_type;
  auto plugin = dunedaq::ipm::make_ipm_sender(plugin_type);
  TLOG_DEBUG(11) << "Connecting sender plugin for connection " << connection_name(connection);
  plugin->connect_for_sends({ { "connection_string", connection.uri } });

  return plugin;
}

} // namespace dunedaq::iomanager
