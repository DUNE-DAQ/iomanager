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

#include "serialization/Serialization.hpp"

#include <functional>
#include <regex>
#include <sstream>

namespace dunedaq {
namespace iomanager {

namespace connection {

enum class QueueType
{
  kUnknown,
  kStdDeQueue,
  kFollySPSCQueue,
  kFollyMPMCQueue
};

enum class ConnectionType
{
  kSendRecv,
  kPubSub
};

struct ConnectionId
{
  std::string uid;
  std::string data_type;
  std::string session = "";
};

struct QueueConfig
{
  ConnectionId id;
  QueueType queue_type;
  uint32_t capacity;
};
typedef std::vector<QueueConfig> Queues_t;

struct Connection
{
  ConnectionId id;
  std::string uri;
  ConnectionType connection_type;
};
typedef std::vector<Connection> Connections_t;

inline QueueType
parse_QueueType(std::string type_name)
{
  if (type_name == "kFollyMPMCQueue")
    return QueueType::kFollyMPMCQueue;
  if (type_name == "kFollySPSCQueue")
    return QueueType::kFollySPSCQueue;
  if (type_name == "kStdDeQueue")
    return QueueType::kStdDeQueue;
  return QueueType::kUnknown;
}

inline ConnectionType
parse_ConnectionType(std::string type)
{
  if (type == "kPubSub")
    return ConnectionType::kPubSub;
 
  return ConnectionType::kSendRecv;
}

inline QueueType
string_to_queue_type(std::string type_name)
{
  auto parsed = parse_QueueType(type_name);
  if (parsed != QueueType::kUnknown) {
    return parsed;
  }

  // StdDeQueue -> kStdDeQueue
  parsed = parse_QueueType("k" + type_name);
  if (parsed != QueueType::kUnknown) {
    return parsed;
  }

  // FollySPSC -> kFollySPSCQueue (same for MPMC)
  return parse_QueueType("k" + type_name + "Queue");
}

inline std::string
str(QueueType const& qtype)
{
  switch (qtype) {
    case QueueType::kFollyMPMCQueue:
      return "kFollyMPMCQueue";
    case QueueType::kFollySPSCQueue:
      return "kFollySPSCQueue";
    case QueueType::kStdDeQueue:
      return "kStdDeQueue";
    default:
    case QueueType::kUnknown:
      return "kUnknown";
  }
  return "kUnknown";
}

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

} // namespace connection

using namespace connection;

} // namespace iomanager

} // namespace dunedaq

namespace std {

template<>
struct hash<dunedaq::iomanager::connection::ConnectionId>
{
  std::size_t operator()(const dunedaq::iomanager::connection::ConnectionId& conn_id) const
  {
    return std::hash<std::string>()(conn_id.session + conn_id.uid + conn_id.data_type);
  }
};

}

#endif // IOMANAGER_INCLUDE_IOMANAGER_SCHEMAUTILS_HPP_