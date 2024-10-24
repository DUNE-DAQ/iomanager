/**
 *
 * @file SchemaUtils.hpp
 *
 * A std::deque-based implementation of Queue
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef IOMANAGER_INCLUDE_IOMANAGER_SCHEMAUTILS_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_SCHEMAUTILS_HPP_

#include "confmodel/Connection.hpp"
#include "confmodel/NetworkConnection.hpp"
#include "confmodel/Service.hpp"

#include <cerrno>
#include <functional>
#include <ifaddrs.h>
#include <netdb.h>
#include <regex>
#include <sstream>

namespace dunedaq {
namespace iomanager {

struct ConnectionId
{
  std::string uid{ "" };
  std::string data_type{ "" };
  std::string session{ "" };

  ConnectionId() {}

  ConnectionId(std::string uid, std::string data_type, std::string session = "")
    : uid(uid)
    , data_type(data_type)
    , session(session)
  {
  }

  explicit ConnectionId(const confmodel::Connection* cfg)
    : uid(cfg->UID())
    , data_type(cfg->get_data_type())
  {
  }
};

inline bool
operator<(ConnectionId const& l, ConnectionId const& r)
{
  if (l.session == r.session || l.session == "" || r.session == "") {
    if (l.data_type == r.data_type) {
      return l.uid < r.uid;
    }
    return l.data_type < r.data_type;
  }
  return l.session < r.session;
}
inline bool
operator==(ConnectionId const& l, ConnectionId const& r)
{
  return (l.session == "" || r.session == "" || l.session == r.session) && l.uid == r.uid && l.data_type == r.data_type;
}

inline bool
is_match(ConnectionId const& search, ConnectionId const& check)
{
  if (search.data_type != check.data_type)
    return false;

  if (search.session != check.session && search.session != "" && check.session != "")
    return false;

  std::regex search_ex(search.uid);
  return std::regex_match(check.uid, search_ex);
}

inline std::string
to_string(const ConnectionId& conn_id)
{
  if (conn_id.session != "") {
    return conn_id.session + "/" + conn_id.uid + "@@" + conn_id.data_type;
  }
  return conn_id.uid + "@@" + conn_id.data_type;
}

inline std::string
get_uri_for_connection(const confmodel::NetworkConnection* netCon)
{
  std::string uri = "";
  if (netCon) {
    TLOG_DEBUG(45) << "Getting URI for network connection " << netCon->UID();
    auto service = netCon->get_associated_service();
    if (service->get_protocol() == "tcp") {

      std::string port = "*";
      if (service->get_port() && service->get_port() != 0) {
        port = std::to_string(service->get_port());
      }
      std::string ipaddr = "0.0.0.0";
      char hostname[256];
      if (gethostname(&hostname[0], 256) == 0) {
        ipaddr = std::string(hostname);
      }
      auto iface = service->get_eth_device_name();
      if (iface != "") {
        // Work out which ip address goes with this device
        struct ifaddrs* ifaddr;
        getifaddrs(&ifaddr);
        for (auto ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
          if (ifa->ifa_addr == NULL) {
            continue;
          }
          if (std::string(ifa->ifa_name) == iface) {
            char ip[NI_MAXHOST];
            int status =
              getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), ip, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (status != 0) {
              continue;
            }
            ipaddr = std::string(ip);
            break;
          }
        }
      }
      uri = std::string(service->get_protocol() + "://" + ipaddr + ":" + port);
    } else if (service->get_protocol() == "inproc") {
      uri = std::string(service->get_protocol() + "://" + service->get_path());
    }
  }
  return uri;
}

} // namespace iomanager

} // namespace dunedaq

namespace std {

template<>
struct hash<dunedaq::iomanager::ConnectionId>
{
  std::size_t operator()(const dunedaq::iomanager::ConnectionId& conn_id) const
  {
    return std::hash<std::string>()(conn_id.session + conn_id.uid + conn_id.data_type);
  }
};

}

#endif // IOMANAGER_INCLUDE_IOMANAGER_SCHEMAUTILS_HPP_
