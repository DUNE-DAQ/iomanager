/**
 * @file ConnectionId.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_CONNECTIONID_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_CONNECTIONID_HPP_

#include "iomanager/connection/Structs.hpp"

#include <sstream>
#include <string>

namespace dunedaq {
namespace iomanager {
namespace connection {
inline bool
operator<(const ConnectionId& l, const ConnectionId& r)
{
  if (l.service_type != r.service_type) {
    return l.service_type < r.service_type;
  }
  if (l.uid != r.uid) {
    return l.uid < r.uid;
  }
  return l.uri < r.uri;
}
inline bool
operator<(const ConnectionRef& l, const ConnectionRef& r)
{
  return l.uid < r.uid;
}

}
// We are doing this here because we really, really want these structs to be in the iomanager namespace!
using namespace connection; // NOLINT

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_CONNECTIONID_HPP_
