/**
 * @file QueueIssues.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_QUEUE_QUEUEISSUES_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_QUEUE_QUEUEISSUES_HPP_

#include "iomanager/CommonIssues.hpp"

#include "ers/Issue.hpp"
#include "logging/Logging.hpp" // NOTE: if ISSUES ARE DECLARED BEFORE include logging/Logging.hpp, TLOG_DEBUG<<issue wont work.

#include <string>

namespace dunedaq {
// Disable coverage collection LCOV_EXCL_START

/**
 * @brief QueueTypeUnknown ERS Issue
 */
ERS_DECLARE_ISSUE(iomanager,
                  QueueTypeUnknown,
                  "Queue type \"" << queue_type << "\" is unknown ",
                  ((std::string)queue_type))

/**
 * @brief QueueTypeMismatch ERS Issue
 */
ERS_DECLARE_ISSUE(iomanager,
                  QueueTypeMismatch,
                  "Requested queue \"" << queue_name << "\" of type '" << target_type << "' already declared as type '"
                                       << source_type << "'",
                  ((std::string)queue_name)((std::string)source_type)((std::string)target_type))

/**
 * @brief QueueNotFound ERS Issue
 */
ERS_DECLARE_ISSUE(iomanager,
                  QueueNotFound,
                  "Requested queue \"" << queue_name << "\" of type '" << target_type << "' could not be found.",
                  ((std::string)queue_name)((std::string)target_type))

/**
 * @brief QueueRegistryConfigured ERS Issue
 */
ERS_DECLARE_ISSUE(iomanager, QueueRegistryConfigured, "QueueRegistry already configured", ERS_EMPTY)

ERS_DECLARE_ISSUE(iomanager,
                  ReceiveCallbackConflict,
                  "QueueReceiverModel for uid " << conn_uid << " is equipped with callback! Ignoring receive call.",
                  ((std::string)conn_uid))

/**
 * @brief QueueTimeoutExpired ERS Issue
 */
ERS_DECLARE_ISSUE(iomanager,
                  QueueTimeoutExpired,
                  name << ": Unable to " << func_name << " within timeout period (timeout period was " << timeout
                       << " milliseconds)",
                  ((std::string)name)((std::string)func_name)((int)timeout)) // NOLINT
// Re-enable coverage collection LCOV_EXCL_STOP

} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_QUEUE_QUEUEISSUES_HPP_
