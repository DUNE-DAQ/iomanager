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

namespace dunedaq::iomanager {
namespace connection {

inline std::string
to_string(Endpoint const& ep)
{
  return ep.data_type + ":" + ep.app_name + ":" + ep.module_name + ":" + str(ep.direction);
}

inline std::string
connection_name(Connection const& c)
{
  return to_string(c.bind_endpoint) + "_Connection";
}

inline QueueType
string_to_queue_type(std::string type_name)
{
  auto parsed = parse_QueueType(type_name);
  if (parsed != QueueType::kUnknown) {
    return parsed;
  }

  return parse_QueueType("k" + type_name);
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