/**
 * @file ConfigClient.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENT_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENT_HPP_

#include "iomanager/SchemaUtils.hpp"
#include "iomanager/network/ConfigClientStructs.hpp"

#include "nlohmann/json.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/version.hpp>

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <thread>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using namespace std::literals::chrono_literals;

namespace dunedaq::iomanager {
class ConfigClient
{
public:
  /**
   * Constructor: Starts a thread that publishes all know connection
   *           information once a second
   *
   * @param server  Name/address of the connection server to publish to
   * @param port    Port on the connection server to connect to
   * @param publish_interval  Time to wait between connection republish (keep-alive)
   */
  ConfigClient(const std::string& server, const std::string& port, std::chrono::milliseconds publish_interval);

  /**
   * Destructor: stops the publishing hread and retracts all published
   *          information
   */
  ~ConfigClient();

  /**
   * Look up a connection in the connection server and return a list
   * of uris that correspond to connection ids that match
   *
   * @param query Query string to send to the server. Query is a
   *    regular expression that can match with multiple connection ids
   * @param session The session that the requested connection is part of
   */
  ConnectionResponse resolveConnection(const ConnectionRequest& query, std::string session = "");

  /**
   * Publish information for a single connection
   * 
   * @param connectionId  The connection Id to be published
   * @param uri           The uri corresponding to the connection id
   */
  void publish(ConnectionRegistration const& connection);
  /**
   * Publish information for multiple connections
   * 
   * @param connectionId A vector of connection Ids to be published
   * @param uri          A vector of uris corresponding to the connection ids.
   *           This vector must be the same length as the connection id vector
   */
  void publish(std::vector<ConnectionRegistration> const& connections);
  /**
   * Retract a single published connection
   */
  void retract(const ConnectionId& connectionId);

  /**
   * Retract multiple published connections
   *
   * @param connectionId  A vector of previously published connection Ids
   *                     to be retracted
   */
  void retract(const std::vector<ConnectionId>& connectionIds);

  /**
   * Retract all connection information that ew have published
   */
  void retract();

  bool is_connected() { return m_connected.load(); }

  private:
  void publish();
  std::string m_session;
  net::io_context m_ioContext;
  net::ip::basic_resolver<net::ip::tcp>::results_type m_addr;

  std::mutex m_mutex;
  std::set<ConnectionRegistration> m_registered_connections;
  std::thread m_thread;
  bool m_active;
  std::atomic<bool> m_connected{ false };
};
} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENT_HPP_
