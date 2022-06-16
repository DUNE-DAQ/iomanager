/**
 * @file NetworkManager.hpp NetworkManager Class implementations
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/NetworkManager.hpp"

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

  for( auto & sender : m_sender_plugins ) {
    opmonlib::InfoCollector tmp_ic;
    sender.second -> get_info( tmp_ic, level );
    ci.add( sender.first, tmp_ic );
  }

  for( auto & receiver : m_receiver_plugins ) {
    opmonlib::InfoCollector tmp_ic;
    receiver.second -> get_info( tmp_ic, level );
    ci.add( receiver.first, tmp_ic );
  }
  
}

void
NetworkManager::configure(const ConnectionIds_t& connections)
{
  if (!m_connection_map.empty()) {
    throw AlreadyConfigured(ERS_HERE);
  }

  for (auto& connection : connections) {
    TLOG_DEBUG(15) << "Adding connection " << connection.uid << " to connection map";
    if (m_connection_map.count(connection.uid) || m_topic_map.count(connection.uid)) {
      TLOG_DEBUG(15) << "Name collision for connection UID " << connection.uid
                     << " connection_map.count: " << m_connection_map.count(connection.uid)
                     << ", topic_map.count: " << m_topic_map.count(connection.uid);
      reset();
      throw NameCollision(ERS_HERE, connection.uid);
    }
    m_connection_map[connection.uid] = connection;
    if (!connection.topics.empty()) {
      for (auto& topic : connection.topics) {
        TLOG_DEBUG(15) << "Adding topic " << topic << " for connection name " << connection.uid << " to topics map";
        if (m_connection_map.count(topic)) {
          reset();
          TLOG_DEBUG(15) << "Name collision with existing connection for topic " << topic << " on connection "
                         << connection.uid;
          throw NameCollision(ERS_HERE, topic);
        }
        m_topic_map[topic].push_back(connection.uid);
      }
    }
  }
}

void
NetworkManager::reset()
{
  std::lock_guard<std::mutex> lk(m_registration_mutex);
  {
    std::lock_guard<std::mutex> lk(m_sender_plugin_map_mutex);
    m_sender_plugins.clear();
  }
  {
    std::lock_guard<std::mutex> lk(m_receiver_plugin_map_mutex);
    m_receiver_plugins.clear();
  }
  m_topic_map.clear();
  m_connection_map.clear();
  m_connection_mutexes.clear();
}

void
NetworkManager::start_publisher(std::string const& connection_name)
{
  TLOG_DEBUG(10) << "Getting connection lock for connection " << connection_name;
  auto send_mutex = get_connection_lock(connection_name);

  if (!m_connection_map.count(connection_name)) {
    throw ConnectionNotFound(ERS_HERE, connection_name);
  }
  if (m_connection_map[connection_name].topics.empty()) {
    throw OperationFailed(ERS_HERE, "Connection is not pub/sub type, cannot start sender early");
  }

  if (!is_connection_open(connection_name, Direction::kOutput)) {
    create_sender(connection_name);
  }
}

std::string
NetworkManager::get_connection_string(std::string const& connection_name) const
{
  if (!m_connection_map.count(connection_name)) {
    throw ConnectionNotFound(ERS_HERE, connection_name);
  }

  return m_connection_map.at(connection_name).uri;
}

std::vector<std::string>
NetworkManager::get_connection_strings(std::string const& topic) const
{
  if (!m_topic_map.count(topic)) {
    throw TopicNotFound(ERS_HERE, topic);
  }

  std::vector<std::string> output;
  for (auto& connection : m_topic_map.at(topic)) {
    output.push_back(m_connection_map.at(connection).uri);
  }

  return output;
}

bool
NetworkManager::is_topic(std::string const& topic) const
{
  if (m_connection_map.count(topic)) {
    return false;
  }
  if (!m_topic_map.count(topic)) {
    return false;
  }

  return true;
}

bool
NetworkManager::is_connection(std::string const& connection_name) const
{
  if (m_topic_map.count(connection_name)) {
    return false;
  }
  if (!m_connection_map.count(connection_name)) {
    return false;
  }

  return true;
}

bool
NetworkManager::is_pubsub_connection(std::string const& connection_name) const
{
  if (is_connection(connection_name)) {
    return !m_connection_map.at(connection_name).topics.empty();
  }

  return false;
}

bool
NetworkManager::is_connection_open(std::string const& connection_name,
                                   Direction direction) const
{
  switch (direction) {
    case Direction::kInput: {
      std::lock_guard<std::mutex> recv_lk(m_receiver_plugin_map_mutex);
      return m_receiver_plugins.count(connection_name);
    }
    case Direction::kOutput: {
      std::lock_guard<std::mutex> send_lk(m_sender_plugin_map_mutex);
      return m_sender_plugins.count(connection_name);
    }
    case Direction::kUnspecified: {
        TLOG_DEBUG(8) << "Connection direction must be specified in is_connection_open!";
    }
  }

  return false;
}

std::shared_ptr<ipm::Receiver>
NetworkManager::get_receiver(std::string const& connection_or_topic)
{
    TLOG_DEBUG(9) << "Getting receiver for connection or topic " << connection_or_topic;
  if (!m_connection_map.count(connection_or_topic) && !m_topic_map.count(connection_or_topic)) {

      TLOG_DEBUG(9) << "Connection not found: " << connection_or_topic << "!";
    throw ConnectionNotFound(ERS_HERE, connection_or_topic);
  }

  if (!is_connection_open(connection_or_topic, Direction::kInput)) {
    TLOG_DEBUG(9) << "Creating receiver for connection or topic " << connection_or_topic;
    create_receiver(connection_or_topic);
  }

  std::shared_ptr<ipm::Receiver> receiver_ptr;
  {
    std::lock_guard<std::mutex> lk(m_receiver_plugin_map_mutex);
    receiver_ptr = m_receiver_plugins[connection_or_topic];
  }

  return receiver_ptr;
}

std::shared_ptr<ipm::Sender>
NetworkManager::get_sender(std::string const& connection_name)
{
  TLOG_DEBUG(10) << "Checking connection map for " << connection_name;
  if (!m_connection_map.count(connection_name)) {
      TLOG_DEBUG(10) << "Connection not found: " << connection_name << "!";
    throw ConnectionNotFound(ERS_HERE, connection_name);
  }

  TLOG_DEBUG(10) << "Checking sender plugins";
  if (!is_connection_open(connection_name, Direction::kOutput)) {
    create_sender(connection_name);
  }

  TLOG_DEBUG(10) << "Sending message";
  std::shared_ptr<ipm::Sender> sender_ptr;
  {
    std::lock_guard<std::mutex> lk(m_sender_plugin_map_mutex);
    sender_ptr = m_sender_plugins[connection_name];
  }

  return sender_ptr;
}

std::shared_ptr<ipm::Subscriber>
NetworkManager::get_subscriber(std::string const& topic)
{
  TLOG_DEBUG(9) << "START";

  if (!m_topic_map.count(topic)) {
    throw ConnectionNotFound(ERS_HERE, topic);
  }

  if (!is_connection_open(topic, Direction::kInput)) {
    TLOG_DEBUG(9) << "Creating receiver for topic " << topic;
    create_receiver(topic);
  }

  std::shared_ptr<ipm::Subscriber> subscriber_ptr;
  {
    std::lock_guard<std::mutex> lk(m_receiver_plugin_map_mutex);
    subscriber_ptr = std::dynamic_pointer_cast<ipm::Subscriber>(m_receiver_plugins[topic]);
  }
  return subscriber_ptr;
}

void
NetworkManager::create_receiver(std::string const& connection_or_topic)
{
  TLOG_DEBUG(12) << "START";
  std::lock_guard<std::mutex> lk(m_receiver_plugin_map_mutex);
  if (m_receiver_plugins.count(connection_or_topic))
    return;

  auto plugin_type = ipm::get_recommended_plugin_name(
    is_topic(connection_or_topic) || is_pubsub_connection(connection_or_topic) ? ipm::IpmPluginType::Subscriber
                                                                               : ipm::IpmPluginType::Receiver);

  TLOG_DEBUG(12) << "Creating plugin for connection or topic " << connection_or_topic << " of type " << plugin_type;
  m_receiver_plugins[connection_or_topic] = dunedaq::ipm::make_ipm_receiver(plugin_type);
  try {
    nlohmann::json config_json;
    if (is_topic(connection_or_topic)) {
      config_json["connection_strings"] = get_connection_strings(connection_or_topic);
    } else {
      config_json["connection_string"] = get_connection_string(connection_or_topic);
    }
    m_receiver_plugins[connection_or_topic]->connect_for_receives(config_json);

    if (is_topic(connection_or_topic)) {
      TLOG_DEBUG(12) << "Subscribing to topic " << connection_or_topic << " after connect_for_receives";
      auto subscriber = std::dynamic_pointer_cast<ipm::Subscriber>(m_receiver_plugins[connection_or_topic]);
      subscriber->subscribe(connection_or_topic);
    }

    if (is_pubsub_connection(connection_or_topic)) {
      TLOG_DEBUG(12) << "Subscribing to topics on " << connection_or_topic << " after connect_for_receives";
      auto subscriber = std::dynamic_pointer_cast<ipm::Subscriber>(m_receiver_plugins[connection_or_topic]);
      for (auto& topic : m_connection_map[connection_or_topic].topics) {
        subscriber->subscribe(topic);
      }
    }

  } catch (ers::Issue const&) {
    m_receiver_plugins.erase(connection_or_topic);
    throw;
  }
  TLOG_DEBUG(12) << "END";
}

void
NetworkManager::create_sender(std::string const& connection_name)
{
  TLOG_DEBUG(11) << "Getting create mutex";
  std::lock_guard<std::mutex> lk(m_sender_plugin_map_mutex);
  TLOG_DEBUG(11) << "Checking plugin list";
  if (m_sender_plugins.count(connection_name))
    return;

  auto plugin_type = ipm::get_recommended_plugin_name(
    is_pubsub_connection(connection_name) ? ipm::IpmPluginType::Publisher : ipm::IpmPluginType::Sender);

  TLOG_DEBUG(11) << "Creating sender plugin for connection " << connection_name << " of type " << plugin_type;
  m_sender_plugins[connection_name] = dunedaq::ipm::make_ipm_sender(plugin_type);
  TLOG_DEBUG(11) << "Connecting sender plugin for connection " << connection_name;
  try {
    m_sender_plugins[connection_name]->connect_for_sends(
      { { "connection_string", m_connection_map[connection_name].uri } });
  } catch (ers::Issue const&) {
    m_sender_plugins.erase(connection_name);
    throw;
  }
}

std::unique_lock<std::mutex>
NetworkManager::get_connection_lock(std::string const& connection_name) const
{
  static std::mutex connection_map_mutex;
  std::unique_lock<std::mutex> lk(connection_map_mutex);
  auto& mut = m_connection_mutexes[connection_name];
  lk.unlock();

  TLOG_DEBUG(13) << "Mutex for connection " << connection_name << " is at " << &mut;
  std::unique_lock<std::mutex> conn_lk(mut);
  return conn_lk;
}

} // namespace dunedaq::iomanager
