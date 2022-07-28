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

#include <functional>
#include <sstream>

namespace dunedaq::iomanager {
namespace connection {

inline std::string
to_string(Endpoint const& ep, bool include_direction = true)
{
  if (include_direction) {

    return ep.data_type + ":" + ep.app_name + ":" + ep.module_name + ":" + str(ep.direction);
  }
  return ep.data_type + ":" + ep.app_name + ":" + ep.module_name;
}

inline std::string
connection_name(Connection const& c)
{
  return to_string(c.bind_endpoint, false) + "_Connection";
}

inline std::string
connection_names(std::vector<Connection> const& cs)
{
  if (cs.size() == 0)
    return "";

  std::ostringstream oss;
  oss << connection_name(cs[0]);

  for (size_t ii = 1; ii < cs.size(); ++ii) {
    oss << ", " << connection_name(cs[ii]);
  }
  return oss.str();
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

inline bool
operator<(const Endpoint& lhs, const Endpoint& rhs)
{
  return to_string(lhs) < to_string(rhs);
}

inline bool
operator==(const Endpoint& lhs, const Endpoint& rhs)
{
  return lhs.data_type == rhs.data_type && lhs.app_name == rhs.app_name && lhs.module_name == rhs.module_name &&
         lhs.direction == rhs.direction && lhs.source_id.subsystem == rhs.source_id.subsystem &&
         lhs.source_id.id == rhs.source_id.id;
}

inline bool
is_match(const Endpoint& search, const Endpoint& check, bool check_direction = true)
{
  if (search == check)
    return true;

  if (search.data_type != check.data_type)
    return false;

  if (search.app_name != "*" && search.app_name != check.app_name) {
    return false;
  }
  if (search.module_name != "*" && search.module_name != check.module_name) {
    return false;
  }
  if (check_direction && search.direction != Direction::kUnspecified && search.direction != check.direction) {
    return false;
  }

  return true;
}

inline bool
is_match(const Endpoint& search, const Connection& check)
{
  if (is_match(search, check.bind_endpoint, false)) {
    return true;
  }
  for (auto& ep : check.connected_endpoints) {
    if (is_match(search, ep)) {
      return true;
    }
  }
  return false;
}

inline bool
is_match(const Endpoint& search, const QueueConfig& check)
{
  for (auto& ep : check.endpoints) {
    if (is_match(search, ep, false)) {
      return true;
    }
  }
  return false;
}

} // namespace connection

using namespace connection;
} // namespace dunedaq::iomanager

namespace std {

template<>
struct hash<dunedaq::iomanager::connection::Endpoint>
{
  std::size_t operator()(const dunedaq::iomanager::connection::Endpoint& endpoint) const
  {
    return std::hash<std::string>()(dunedaq::iomanager::connection::to_string(endpoint));
  }
};
}

#endif // IOMANAGER_INCLUDE_IOMANAGER_SCHEMAUTILS_HPP_