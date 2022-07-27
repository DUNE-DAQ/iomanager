/**
 * @file CommonIssues.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_COMMONISSUES_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_COMMONISSUES_HPP_

#include "ers/Issue.hpp"

#include <string>

namespace dunedaq {
// Disable coverage collection LCOV_EXCL_START
ERS_DECLARE_ISSUE(iomanager,
                  ConnectionInstanceNotFound,
                  "Connection Instance not found for name " << name,
                  ((std::string)name))

ERS_DECLARE_ISSUE(iomanager,      // namespace
                  TimeoutExpired, // issue class name
                  name << ": Unable to " << func_name << " within timeout period (timeout period was " << timeout
                       << " milliseconds)",                                  // message
                  ((std::string)name)((std::string)func_name)((int)timeout)) // NOLINT(readability/casting)

ERS_DECLARE_ISSUE(iomanager, OperationFailed, message, ((std::string)message))

ERS_DECLARE_ISSUE(iomanager,
                  NetworkMessageNotSerializable,
                  "Object of type " << type << " is not serializable but configured for network transfer!",
                  ((std::string)type))

ERS_DECLARE_ISSUE(iomanager, ConnectionNotFound, "Connection named " << name << " not found", ((std::string)name))
ERS_DECLARE_ISSUE(iomanager, TopicNotFound, "Topic named " << name << " not found", ((std::string)name))
ERS_DECLARE_ISSUE(iomanager,
                  ConnectionTopicNotFound,
                  "Topic named " << name << " not found for connection " << connection,
                  ((std::string)name)((std::string)connection))
ERS_DECLARE_ISSUE(iomanager, NameCollision, "Multiple instances of name " << name << " exist", ((std::string)name))

ERS_DECLARE_ISSUE(iomanager,
                  ConnectionAlreadyOpen,
                  "Connection named " << name << " has already been opened for " << direction,
                  ((std::string)name)((std::string)direction))
ERS_DECLARE_ISSUE(iomanager,
                  ConnectionNotOpen,
                  "Connection named " << name << " is not open for " << direction,
                  ((std::string)name)((std::string)direction))
ERS_DECLARE_ISSUE(iomanager, AlreadyConfigured, "The NetworkManager has already been configured", )
ERS_DECLARE_ISSUE(iomanager,
                  ConnectionDirectionMismatch,
                  "Connection reference with name " << name << " specified direction " << direction
                                                    << ", but tried to obtain a " << handle_type,
                  ((std::string)name)((std::string)direction)((std::string)handle_type))
// Re-enable coverage collection LCOV_EXCL_STOP

} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_COMMONISSUES_HPP_
