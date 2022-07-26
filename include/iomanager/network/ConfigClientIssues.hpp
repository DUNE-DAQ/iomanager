/**
 * @file ConfigClientIssues.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENTISSUES_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENTISSUES_HPP_

#include "ers/Issue.hpp"

#include <string>

namespace dunedaq {
ERS_DECLARE_ISSUE(iomanager, EnvNotFound, "Environment variable " << name << " not found", ((std::string)name))
ERS_DECLARE_ISSUE(iomanager, FailedPublish, "Failed to publish configuration " << result, ((std::string)result))
ERS_DECLARE_ISSUE(iomanager,
                  FailedRetract,
                  "Failed to retract configuration " << result,
                  ((std::string)name)((std::string)result))
ERS_DECLARE_ISSUE(iomanager,
                  FailedLookup,
                  "Failed to lookup " << target << " " << result,
                  ((std::string)target)((std::string)result))
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENTISSUES_HPP_
