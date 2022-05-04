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

ERS_DECLARE_ISSUE(iomanager,
		  OperationFailed,
		  message,
		  ((std::string)message))  
// Re-enable coverage collection LCOV_EXCL_STOP

} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_COMMONISSUES_HPP_
