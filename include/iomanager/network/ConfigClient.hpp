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

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>

namespace dunedaq::iomanager {
class ConfigClient
{
public:
  ConfigClient(const std::string& server, const std::string& port);
  ~ConfigClient();

  ConnectionResponse resolveEndpoint(ConnectionRequest const& request);
  std::string resolveConnection(const Connection& connection);

  void publishEndpoint(const Endpoint& endpoint, const std::string& uri,
                       const std::string& connection_type="");
  void publishConnection(const Connection& connection);
  void retract(const Endpoint& endpoint);
  void retract(const Connection& connection);

private:
  std::string get(const std::string& target, const std::string& params);
  void publish(const std::string& content);
  void retract(const std::string& content);

  nlohmann::json jsonify(const Connection& connection);

  std::string m_partition;
  net::io_context m_ioContext;
  net::ip::basic_resolver<net::ip::tcp>::results_type m_addr;
  beast::tcp_stream m_stream;
  beast::flat_buffer m_buffer;
};
} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENT_HPP_
