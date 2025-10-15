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
#include "utilities/get_ips.hpp"

#include <functional>
#include <regex>
#include <string>

namespace dunedaq::iomanager {

struct ConnectionId
{
  std::string uid{ "" };
  std::string data_type{ "" };
  std::string tag{ "" };
  std::string session{ "" };

  ConnectionId() {}

  ConnectionId(std::string uid, std::string data_type, std::string tag = "", std::string session = "")
    : uid(uid)
    , data_type(data_type)
    , tag(tag)
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
      if (l.uid == r.uid) {
        return l.tag < r.tag;
      }
      return l.uid < r.uid;
    }
    return l.data_type < r.data_type;
  }
  return l.session < r.session;
}
inline bool
operator==(ConnectionId const& l, ConnectionId const& r)
{
  return (l.session == "" || r.session == "" || l.session == r.session) && l.uid == r.uid && l.tag == r.tag &&
         l.data_type == r.data_type;
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
    return conn_id.session + "/" + conn_id.uid + (conn_id.tag != "" ? "+" + conn_id.tag : "") + "@@" +
           conn_id.data_type;
  }
  return conn_id.uid + (conn_id.tag != "" ? "+" + conn_id.tag : "") + "@@" + conn_id.data_type;
}

inline std::string
get_uri_for_connection(const confmodel::NetworkConnection* netCon)
{
  std::string uri = "";
  if (netCon) {
    TLOG_DEBUG(45) << "Getting URI for network connection " << netCon->UID();
    auto service = netCon->get_associated_service();
    auto protocol = service->get_protocol();
    if (protocol == "tcp") {

      std::string port = "*";
      if (service->get_port() && service->get_port() != 0) {
        port = std::to_string(service->get_port());
      }
      auto iface = service->get_eth_device_name();
      uri = std::string(service->get_protocol() + "://" + dunedaq::utilities::get_interface_ip(iface) + ":" + port);
    } else if (protocol == "inproc") {
      uri = std::string(service->get_protocol() + "://" + service->get_path());
    }
  }
  return uri;
}

} // namespace dunedaq::iomanager

namespace std {

template<>
struct hash<dunedaq::iomanager::ConnectionId>
{
  std::size_t operator()(const dunedaq::iomanager::ConnectionId& conn_id) const
  {
    return std::hash<std::string>()(conn_id.session + conn_id.uid + conn_id.tag + conn_id.data_type);
  }
};

} // namespace std

#endif // IOMANAGER_INCLUDE_IOMANAGER_SCHEMAUTILS_HPP_
