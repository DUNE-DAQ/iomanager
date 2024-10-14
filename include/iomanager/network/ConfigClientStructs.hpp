/**
 * @file ConfigClientStructs.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENTSTRUCTS_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENTSTRUCTS_HPP_

#include "iomanager/SchemaUtils.hpp"
#include "serialization/Serialization.hpp"

#include <string>

namespace dunedaq {
namespace iomanager {

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
  ConnectionRequest(ConnectionId convert)
    : uid_regex(convert.uid)
    , data_type(convert.data_type)
  {
  }

  DUNE_DAQ_SERIALIZE(ConnectionRequest, uid_regex, data_type);
};

struct ConnectionInfo
{
  std::string uid;
  std::string data_type;
  std::string uri;
  ConnectionType connection_type; // Maps to dunedaq::confmodel::NetworkConnection::Connection_type

  ConnectionInfo() {}

  // Implicit Conversion
  ConnectionInfo(const confmodel::NetworkConnection* convert)
    : uid(convert->UID())
    , data_type(convert->get_data_type())
    , uri(get_uri_for_connection(convert))
    , connection_type(string_to_connection_type_enum(convert->get_connection_type()))
  {
  }

  DUNE_DAQ_SERIALIZE(ConnectionInfo, uid, data_type, uri, connection_type);
};

struct ConnectionRegistration
{
  std::string uid;
  std::string data_type;
  std::string uri;
  ConnectionType connection_type; // Maps to dunedaq::confmodel::NetworkConnection::Connection_type

  ConnectionRegistration() {}

  // Implicit Conversion
  ConnectionRegistration(const confmodel::NetworkConnection* convert)
    : uid(convert->UID())
    , data_type(convert->get_data_type())
    , uri(get_uri_for_connection(convert))
    , connection_type(string_to_connection_type_enum(convert->get_connection_type()))
  {
  }

  // Implicit Conversion
  ConnectionRegistration(ConnectionInfo convert)
    : uid(convert.uid)
    , data_type(convert.data_type)
    , uri(convert.uri)
    , connection_type(convert.connection_type)
  {
  }

  DUNE_DAQ_SERIALIZE(ConnectionRegistration, uid, data_type, uri, connection_type);
};

struct ConnectionResponse
{
  std::vector<ConnectionInfo> connections;

  DUNE_DAQ_SERIALIZE(ConnectionResponse, connections);
};

inline bool
operator<(ConnectionRegistration const& l, ConnectionRegistration const& r)
{
  if (l.data_type == r.data_type) {
    return l.uid < r.uid;
  }
  return l.data_type < r.data_type;
}
}
}

MSGPACK_ADD_ENUM(dunedaq::iomanager::ConnectionType)

#endif // IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENTSTRUCTS_HPP_
