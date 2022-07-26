#ifndef CONFIGCLIENT_HPP
#define CONFIGCLIENT_HPP

#include <string>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/version.hpp>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>

namespace dunedaq {
namespace iomanager {
class ConfigClient
{
public:
  ConfigClient(const std::string& server, const std::string& port);
  ~ConfigClient();

  void publishApp(const std::string& name, const std::string& config, const std::string& sources = "");
  void publishConnection(const std::string& config, const std::string& sources = "");
  void retract(const std::string& name);
  std::string getAppConfig(const std::string& appName);
  std::string getSourceApp(const std::string& source);
  std::string getSourceConnection(const std::string& source);
  std::string getConnectionConfig(const std::string& connection);

private:
  std::string get(const std::string& target);
  void publish(const std::string& content);

  std::string m_partition;
  net::io_context m_ioContext;
  net::ip::basic_resolver<net::ip::tcp>::results_type m_addr;
  beast::tcp_stream m_stream;
  beast::flat_buffer m_buffer;
};
} // iomanager
} // dunedaq
#endif
