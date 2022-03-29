/**
 * @file ConnectionID.hpp
 * 
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_CONNECTIONID_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_CONNECTIONID_HPP_

#include <sstream>
#include <string>

namespace dunedaq {
namespace iomanager {

struct ConnectionID
{
  std::string m_service_type;
  std::string m_service_name;
  std::string m_topic;
};

inline bool
operator<(const ConnectionID& l, const ConnectionID& r)
{
  std::ostringstream ossl;
  std::ostringstream ossr;
  ossl << l.m_service_type << l.m_service_type << l.m_topic;
  ossr << r.m_service_type << r.m_service_type << r.m_topic;
  return ossl.str() < ossr.str();
}

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_CONNECTIONID_HPP_
