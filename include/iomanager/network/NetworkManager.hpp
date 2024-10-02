/**
 *
 * @file NetworkManager.hpp NetworkManager Singleton class
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_NETWORKMANAGER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_NETWORKMANAGER_HPP_

#include "iomanager/network/NetworkIssues.hpp"
#include "iomanager/connection/Structs.hpp"
#include "iomanager/network/ConfigClient.hpp"

#include "ipm/Receiver.hpp"
#include "ipm/Sender.hpp"
#include "ipm/Subscriber.hpp"
#include "opmonlib/OpMonManager.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dunedaq::iomanager {

using namespace connection;

class NetworkManager
{

public:
  static NetworkManager& get();
  ~NetworkManager() { reset(); }

  void configure(const Connections_t& connections, bool use_config_client, std::chrono::milliseconds config_client_interval,
		 dunedaq::opmonlib::OpMonManager &);
  void reset();
  void shutdown();

  std::shared_ptr<ipm::Receiver> get_receiver(ConnectionId const& conn_id);
  std::shared_ptr<ipm::Sender> get_sender(ConnectionId const& conn_id);

  void remove_sender(ConnectionId const& conn_id);

  bool is_pubsub_connection(ConnectionId const& conn_id) const;

  ConnectionResponse get_connections(ConnectionId const& conn_id, bool restrict_single = false) const;
  ConnectionResponse get_preconfigured_connections(ConnectionId const& conn_id) const;

  std::set<std::string> get_datatypes(std::string const& uid) const;

private:
  static std::unique_ptr<NetworkManager> s_instance;

  NetworkManager() = default;

  NetworkManager(NetworkManager const&) = delete;
  NetworkManager(NetworkManager&&) = delete;
  NetworkManager& operator=(NetworkManager const&) = delete;
  NetworkManager& operator=(NetworkManager&&) = delete;

  std::shared_ptr<ipm::Receiver> create_receiver(std::vector<ConnectionInfo> connections, ConnectionId const& conn_id);
  std::shared_ptr<ipm::Sender> create_sender(ConnectionInfo connection);

  void update_subscribers();

  std::unordered_map<ConnectionId, Connection> m_preconfigured_connections;
  std::unordered_map<ConnectionId, std::shared_ptr<ipm::Receiver>> m_receiver_plugins;
  std::unordered_map<ConnectionId, std::shared_ptr<ipm::Sender>> m_sender_plugins;
  std::shared_ptr<dunedaq::opmonlib::OpMonLink> m_sender_opmon_link{ std::make_shared<dunedaq::opmonlib::OpMonLink>() };
  std::shared_ptr<dunedaq::opmonlib::OpMonLink> m_receiver_opmon_link{ std::make_shared<dunedaq::opmonlib::OpMonLink>() };
  static void register_monitorable_node( std::shared_ptr<opmonlib::MonitorableObject> conn,
					 std::shared_ptr<opmonlib::OpMonLink> link,
					 const std::string & name, bool is_pubsub );
  
  std::unordered_map<ConnectionId, std::shared_ptr<ipm::Subscriber>> m_subscriber_plugins;
  std::unique_ptr<std::thread> m_subscriber_update_thread;
  std::atomic<bool> m_subscriber_update_thread_running{ false };

  std::unique_ptr<ConfigClient> m_config_client;
  std::chrono::milliseconds m_config_client_interval;

  mutable std::mutex m_receiver_plugin_map_mutex;
  mutable std::mutex m_sender_plugin_map_mutex;
  mutable std::mutex m_subscriber_plugin_map_mutex;
};
} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_NETWORKMANAGER_HPP_
