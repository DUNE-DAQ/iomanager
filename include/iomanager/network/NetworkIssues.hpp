/**
 * @file NetworkIssues.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_NETWORK_NETWORKISSUES_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_NETWORK_NETWORKISSUES_HPP_

#include "ers/Issue.hpp"
#include "iomanager/CommonIssues.hpp"

#include <string>

namespace dunedaq {
// Disable coverage collection LCOV_EXCL_START

ERS_DECLARE_ISSUE(iomanager,
                  NetworkMessageNotSerializable,
                  "Object of type " << type << " is not serializable but configured for network transfer!",
                  ((std::string)type))

ERS_DECLARE_ISSUE(iomanager, ConnectionNotFound, "Connection named " << cuid << " of type " << data_type << " not found", ((std::string)cuid)((std::string)data_type))

ERS_DECLARE_ISSUE(iomanager, NameCollision, "Multiple instances of name " << name << " exist", ((std::string)name))

ERS_DECLARE_ISSUE(iomanager, AlreadyConfigured, "The NetworkManager has already been configured", )

ERS_DECLARE_ISSUE(iomanager, EnvNotFound, "Environment variable " << name << " not found", ((std::string)name))

ERS_DECLARE_ISSUE(iomanager, FailedPublish, "Failed to publish configuration " << result, ((std::string)result))

ERS_DECLARE_ISSUE(iomanager,
                  FailedRetract,
                  "Failed to retract configuration " << result,
                  ((std::string)name)((std::string)result))

    ERS_DECLARE_ISSUE(iomanager,
                  FailedLookup,
                  "Failed to lookup " << cuid << " at " << target << " " << result,
                  ((std::string)cuid)((std::string)target)((std::string)result))

    ERS_DECLARE_ISSUE(iomanager,
                      PublishException,
                      "Caught exception <" << exc << "> while trying to publish",
                      ((std::string)exc))


// Re-enable coverage collection LCOV_EXCL_STOP

} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_NETWORK_NETWORKISSUES_HPP_
