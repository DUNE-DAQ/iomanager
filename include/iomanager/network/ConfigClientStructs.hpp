/**
 * @file ConfigClientStructs.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENTSTRUCTS_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENTSTRUCTS_HPP_

#include "iomanager/SchemaUtils.hpp"

#include <string>

namespace dunedaq {
namespace iomanager {

struct ConnectionRegistration
{
  Connection data;
};

struct ConnectionRequest
{
  std::string data_type;
  std::string uid;

  // Implicit conversion
  ConnectionRequest(ConnectionId convert)
    : data_type(convert.data_type)
    , uid(convert.uid)
  {
  }
};

struct ConnectionResponse
{
  Connections_t connections;
};
}
}

#endif // IOMANAGER_INCLUDE_IOMANAGER_CONFIGCLIENTSTRUCTS_HPP_
