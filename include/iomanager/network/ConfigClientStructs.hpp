/**
 * @file ConfigClientStructs.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_NETWORK_CONFIGCLIENTSTRUCTS_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_NETWORK_CONFIGCLIENTSTRUCTS_HPP_

#include "iomanager/SchemaUtils.hpp"
#include "nlohmann/json.hpp"

#include <string>
#include <vector>

namespace dunedaq::iomanager {

enum class ConnectionType : int
{
  kSendRecv = 0,
  kPubSub = 1,
  kInvalid = 2,
};

inline ConnectionType
string_to_connection_type_enum(std::string type)
{
  if (type == confmodel::NetworkConnection::Connection_type::KPubSub)
    return ConnectionType::kPubSub;
  if (type == confmodel::NetworkConnection::Connection_type::KSendRecv)
    return ConnectionType::kSendRecv;

  return ConnectionType::kInvalid;
}

struct ConnectionRequest
{
  std::string uid_regex;
  std::string data_type;

  ConnectionRequest() {}

  // Implicit conversion
  ConnectionRequest(ConnectionId convert) // NOLINT(runtime/explicit)
    : uid_regex(convert.uid)
    , data_type(convert.data_type)
  {
  }
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConnectionRequest, uid_regex, data_type);
};

struct ConnectionInfo
{
  std::string uid;
  std::string data_type;
  int capacity;
  std::string uri;
  ConnectionType connection_type; // Maps to dunedaq::confmodel::NetworkConnection::Connection_type

  ConnectionInfo() {}

  // Implicit Conversion
  ConnectionInfo(const confmodel::NetworkConnection* convert) // NOLINT(runtime/explicit)
    : uid(convert->UID())
    , data_type(convert->get_data_type())
    , capacity(convert->get_capacity())
    , uri(get_uri_for_connection(convert))
    , connection_type(string_to_connection_type_enum(convert->get_connection_type()))
  {
  }

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConnectionInfo, uid, data_type, capacity, uri, connection_type);
};

struct ConnectionRegistration
{
  std::string uid;
  std::string data_type;
  int capacity;
  std::string uri;
  ConnectionType connection_type; // Maps to dunedaq::confmodel::NetworkConnection::Connection_type

  ConnectionRegistration() {}

  // Implicit Conversion
  ConnectionRegistration(const confmodel::NetworkConnection* convert) // NOLINT(runtime/explicit)
    : uid(convert->UID())
    , data_type(convert->get_data_type())
    , capacity(convert->get_capacity())
    , uri(get_uri_for_connection(convert))
    , connection_type(string_to_connection_type_enum(convert->get_connection_type()))
  {
  }

  // Implicit Conversion
  ConnectionRegistration(ConnectionInfo convert) // NOLINT(runtime/explicit)
    : uid(convert.uid)
    , data_type(convert.data_type)
    , capacity(convert.capacity)
    , uri(convert.uri)
    , connection_type(convert.connection_type)
  {
  }

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConnectionRegistration, uid, data_type, capacity, uri, connection_type);
};

struct ConnectionResponse
{
  std::vector<ConnectionInfo> connections;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConnectionResponse, connections);
};

inline bool
operator<(ConnectionRegistration const& l, ConnectionRegistration const& r)
{
  if (l.data_type == r.data_type) {
    return l.uid < r.uid;
  }
  return l.data_type < r.data_type;
}

} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_NETWORK_CONFIGCLIENTSTRUCTS_HPP_
