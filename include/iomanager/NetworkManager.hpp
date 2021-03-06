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
#include "iomanager/ConnectionId.hpp"

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

namespace dunedaq {

namespace iomanager {
class NetworkManager
{

public:
  static NetworkManager& get();

  void gather_stats(opmonlib::InfoCollector& ci, int /*level*/);
  void configure(const ConnectionIds_t& connections);
  void reset();

  // Receive via callback
  void start_listening(std::string const& connection_name);
  void stop_listening(std::string const& connection_name);
  void clear_callback(std::string const& connection_or_topic);
  void subscribe(std::string const& topic);
  void unsubscribe(std::string const& topic);

  // Direct Send/Receive
  void start_publisher(std::string const& connection_name);

  std::string get_connection_string(std::string const& connection_name) const;
  std::vector<std::string> get_connection_strings(std::string const& topic) const;

  bool is_topic(std::string const& topic) const;
  bool is_connection(std::string const& connection_name) const;
  bool is_pubsub_connection(std::string const& connection_name) const;
  bool is_listening(std::string const& connection_or_topic) const;

  bool is_connection_open(std::string const& connection_name, Direction direction = Direction::kInput) const;

  std::shared_ptr<ipm::Receiver> get_receiver(std::string const& connection_or_topic);
  std::shared_ptr<ipm::Sender> get_sender(std::string const& connection_name);
  std::shared_ptr<ipm::Subscriber> get_subscriber(std::string const& topic);

private:
  static std::unique_ptr<NetworkManager> s_instance;

  NetworkManager() = default;

  NetworkManager(NetworkManager const&) = delete;
  NetworkManager(NetworkManager&&) = delete;
  NetworkManager& operator=(NetworkManager const&) = delete;
  NetworkManager& operator=(NetworkManager&&) = delete;

  bool is_listening_locked(std::string const& connection_or_topic) const;
  void create_receiver(std::string const& connection_or_topic);
  void create_sender(std::string const& connection_name);

  std::unordered_map<std::string, ConnectionId> m_connection_map;
  std::unordered_map<std::string, std::vector<std::string>> m_topic_map;
  std::unordered_map<std::string, std::shared_ptr<ipm::Receiver>> m_receiver_plugins;
  std::unordered_map<std::string, std::shared_ptr<ipm::Sender>> m_sender_plugins;

  std::unique_lock<std::mutex> get_connection_lock(std::string const& connection_name) const;
  mutable std::unordered_map<std::string, std::mutex> m_connection_mutexes;
  mutable std::mutex m_receiver_plugin_map_mutex;
  mutable std::mutex m_sender_plugin_map_mutex;
  mutable std::mutex m_registration_mutex;
};
} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_NETWORKMANAGER_HPP_
