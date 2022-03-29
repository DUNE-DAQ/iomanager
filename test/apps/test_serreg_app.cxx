/**
 * @file test_serreg_app.cxx Test application for
 * SerializerRegistry usage.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#include "logging/Logging.hpp"

#include "iomanager/SerializerRegistry.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

std::string
int_to_string(int num)
{
  std::stringstream ss;
  ss << num;
  return ss.str();
}

int
string_to_int(std::string str)
{
  std::stringstream ss(str);
  int num = 0;
  ss >> num;
  return num;
}

int
main(int /*argc*/, char** /*argv[]*/)
{
  // SerializerRegistry
  dunedaq::iomanager::SerializerRegistry serreg;

  // Register serializers for integer
  serreg.register_serializer<int>(int_to_string);
  serreg.register_deserializer<int>(string_to_int);

  // Serialize
  std::string as_str = serreg.get_serializer<int>().call<std::string>(5);
  TLOG() << "Serialized: " << as_str;

  // Deserialize
  int as_int = serreg.get_deserializer<int>().call<int>(std::string(as_str));
  TLOG() << "Deserialized: " << as_int;

  // Access to GenericCallback for re-use
  auto& int_deser = serreg.get_deserializer<int>();
  // std::any args
  std::vector<std::any> args = { { std::string("12345") } };
  // call with std::any args
  std::any result = int_deser.callAny(args);
  TLOG() << "result for any cast = " << std::any_cast<int>(result);

  // Exit
  TLOG() << "Exiting.";
  return 0;
}
