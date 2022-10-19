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
   */
  ConfigClient(const std::string& server, const std::string& port);

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
   */
  std::vector<std::string> resolveConnection(const std::string& query);

  /**
   * Publish information for a single connection
   * 
   * @param connectionId  The connection Id to be published
   * @param uri           The uri corresponding to the connection id
   */
  void publish(const std::string& connectionId,
               const std::string& uri);
  /**
   * Publish information for multiple connections
   * 
   * @param connectionId A vector of connection Ids to be published
   * @param uri          A vector of uris corresponding to the connection ids.
   *           This vector must be the same length as the connection id vector
   */
  void publish(const std::vector<std::string>& connectionId,
               const std::vector<std::string>& uri);
  /**
   * Retract a single published connection
   */
  void retract(const std::string& connectionId);

  /**
   * Retract multiple published connections
   *
   * @param connectionId  A vector of previously published connection Ids
   *                     to be retracted
   */
  void retract(const std::vector<std::string>& connectionId);

  /**
   * Retract all connection information that ew have published
   */
  void retract();

private:
  void publish();
  std::string m_partition;
  net::io_context m_ioContext;
  net::ip::basic_resolver<net::ip::tcp>::results_type m_addr;
  beast::tcp_stream m_stream;
  beast::flat_buffer m_buffer;

  std::mutex m_mutex;
  std::map<std::string,std::string> m_connectionMap;
  std::thread m_thread;
  bool m_active;
};
} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENT_HPP_
