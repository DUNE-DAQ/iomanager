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

#include "iomanager/connection/Structs.hpp"
#include "serialization/Serialization.hpp"

#include <functional>
#include <regex>
#include <sstream>

namespace dunedaq {
namespace iomanager {

namespace connection {

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

inline bool
operator<(ConnectionId const& l, ConnectionId const& r)
{
  if (l.data_type == r.data_type) {
    return l.uid < r.uid;
  }
  return l.data_type < r.data_type;
}
inline bool
operator==(ConnectionId const& l, ConnectionId const& r)
{
  return l.uid == r.uid && l.data_type == r.data_type;
}

inline bool
is_match(ConnectionId const& search, ConnectionId const& check)
{
  if (search.data_type != check.data_type)
    return false;

  std::regex search_ex(search.uid);
  return std::regex_match(check.uid, search_ex);
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
    return std::hash<std::string>()(conn_id.uid + conn_id.data_type);
  }
};
}

#endif // IOMANAGER_INCLUDE_IOMANAGER_SCHEMAUTILS_HPP_