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

#include "iomanager/CommonIssues.hpp"
#include "iomanager/connection/Structs.hpp"
#include "iomanager/network/ConfigClient.hpp"

#include "ipm/Receiver.hpp"
#include "ipm/Sender.hpp"
#include "ipm/Subscriber.hpp"
#include "opmonlib/InfoCollector.hpp"

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

  void gather_stats(opmonlib::InfoCollector& ci, int /*level*/);
  void configure(const Connections_t& connections);
  void reset();

  std::shared_ptr<ipm::Receiver> get_receiver(ConnectionId const& conn_id);
  std::shared_ptr<ipm::Sender> get_sender(ConnectionId const& conn_id);

  bool is_pubsub_connection(ConnectionId const& conn_id) const;

  std::string GetUriForConnection(Connection conn);
  ConnectionResponse get_preconfigured_connections(ConnectionId const& conn_id) const;

private:
  static std::unique_ptr<NetworkManager> s_instance;

  NetworkManager() = default;

  NetworkManager(NetworkManager const&) = delete;
  NetworkManager(NetworkManager&&) = delete;
  NetworkManager& operator=(NetworkManager const&) = delete;
  NetworkManager& operator=(NetworkManager&&) = delete;

  std::shared_ptr<ipm::Receiver> create_receiver(Connections_t connections);
  std::shared_ptr<ipm::Sender> create_sender(Connection connection);

  std::unordered_map<std::string, Connection> m_preconfigured_connections;
  std::unordered_map<ConnectionId, std::shared_ptr<ipm::Receiver>> m_receiver_plugins;
  std::unordered_map<ConnectionId, std::shared_ptr<ipm::Sender>> m_sender_plugins;

  std::unique_ptr<ConfigClient> m_config_client;

  mutable std::mutex m_receiver_plugin_map_mutex;
  mutable std::mutex m_sender_plugin_map_mutex;
};
} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_NETWORKMANAGER_HPP_
