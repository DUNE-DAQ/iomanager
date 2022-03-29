/**
 * @file SerializerRegistry.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_SERIALIZERREGISTRY_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_SERIALIZERREGISTRY_HPP_

#include "iomanager/GenericCallback.hpp"

#include <functional>
#include <map>
#include <typeindex>
#include <typeinfo>

namespace dunedaq {
namespace iomanager {

class SerializerRegistry
{
public:
  SerializerRegistry() {}
  SerializerRegistry(const SerializerRegistry&) = delete;            ///< SerializerRegistry is not copy-constructible
  SerializerRegistry& operator=(const SerializerRegistry&) = delete; ///< SerializerRegistry is not copy-assignable
  SerializerRegistry(SerializerRegistry&&) = delete;                 ///< SerializerRegistry is not move-constructible
  SerializerRegistry& operator=(SerializerRegistry&&) = delete;      ///< SerializerRegistry is not move-assignable

  template<typename Datatype, typename Function>
  void register_serializer(Function&& f)
  {
    m_serializers[std::type_index(typeid(Datatype))] = f; // NOLINT
  }

  template<typename Datatype, typename Function>
  void register_deserializer(Function&& f)
  {
    m_deserializers[std::type_index(typeid(Datatype))] = f; // NOLINT
  }

  template<typename Datatype>
  GenericCallback& get_serializer()
  {
    auto tidx = std::type_index(typeid(Datatype)); // NOLINT
    if (!m_serializers.count(tidx)) {
      // throw error
    }
    return m_serializers[tidx];
  }

  template<typename Datatype>
  GenericCallback& get_deserializer()
  {
    auto tidx = std::type_index(typeid(Datatype)); // NOLINT
    if (!m_deserializers.count(tidx)) {
      // throw error
    }
    return m_deserializers[tidx];
  }

private:
  std::map<std::type_index, GenericCallback> m_serializers;
  std::map<std::type_index, GenericCallback> m_deserializers;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_SERIALIZERREGISTRY_HPP_
