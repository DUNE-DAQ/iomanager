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

namespace dunedaq::iomanager {
namespace connection {
std::string
to_string(Endpoint const& ep)
{
  return ep.data_type + ":" + ep.app_name + ":" + ep.module_name + ":" + str(ep.direction);
}

QueueType
string_to_queue_type(std::string type_name)
{
  auto parsed = parse_QueueType(type_name);
  if (parsed != QueueType::kUnknown) {
    return parsed;
  }

  return parse_QueueType("k" + type_name);
}
} // namespace connection

using namespace connection;
} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_SCHEMAUTILS_HPP_