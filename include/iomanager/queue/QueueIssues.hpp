/**
 * @file QueueIssues.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_QUEUE_QUEUEISSUES_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_QUEUE_QUEUEISSUES_HPP_

#include "ers/Issue.hpp"
#include "iomanager/CommonIssues.hpp"

#include <string>

namespace dunedaq {
// Disable coverage collection LCOV_EXCL_START

/**
 * @brief QueueTypeUnknown ERS Issue
 */
ERS_DECLARE_ISSUE(iomanager,        // namespace
                  QueueTypeUnknown, // issue class name
                  "Queue type \"" << queue_type << "\" is unknown ",
                  ((std::string)queue_type))

/**
 * @brief QueueTypeMismatch ERS Issue
 */
ERS_DECLARE_ISSUE(iomanager,         // namespace
                  QueueTypeMismatch, // issue class name
                  "Requested queue \"" << queue_name << "\" of type '" << target_type << "' already declared as type '"
                                       << source_type << "'", // message
                  ((std::string)queue_name)((std::string)source_type)((std::string)target_type))

/**
 * @brief QueueNotFound ERS Issue
 */
ERS_DECLARE_ISSUE(iomanager,     // namespace
                  QueueNotFound, // issue class name
                  "Requested queue \"" << queue_name << "\" of type '" << target_type
                                       << "' could not be found.", // message
                  ((std::string)queue_name)((std::string)target_type))

/**
 * @brief QueueRegistryConfigured ERS Issue
 */
ERS_DECLARE_ISSUE(iomanager,               // namespace
                  QueueRegistryConfigured, // issue class name
                  "QueueRegistry already configured",
                  ERS_EMPTY)

ERS_DECLARE_ISSUE(iomanager,
                  ReceiveCallbackConflict,
                  "QueueReceiverModel for uid " << conn_uid << " is equipped with callback! Ignoring receive call.",
                  ((std::string)conn_uid))

ERS_DECLARE_ISSUE(iomanager,
                  CrossSessionQueue,
                  "This application is in session " << app_session << ", and cannot use a queue configured for session "
                                                    << queue_session << "! Queue ID " << queue_id,
                  ((std::string)app_session)((std::string)queue_session)((std::string)queue_id))

// Re-enable coverage collection LCOV_EXCL_STOP

} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_QUEUE_QUEUEISSUES_HPP_
