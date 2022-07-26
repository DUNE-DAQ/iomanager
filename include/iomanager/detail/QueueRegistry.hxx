#include "iomanager/FollyQueue.hpp"
#include "iomanager/StdDeQueue.hpp"

#include <cxxabi.h>
#include <memory>

// Declarations
namespace dunedaq::iomanager {

template<typename T>
std::shared_ptr<Queue<T>>
QueueRegistry::get_queue(const std::string& name)
{

  auto queue_it = m_queue_registry.find(name);
  if (queue_it != m_queue_registry.end()) {
    auto queuePtr = std::dynamic_pointer_cast<Queue<T>>(queue_it->second.m_instance);

    if (!queuePtr) {
      // TODO: John Freeman (jcfree@fnal.gov), Jun-23-2020. Add checks for demangling status. Timescale 2 weeks.
      int status = -999;
      std::string realname_target = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status);
      std::string realname_source = abi::__cxa_demangle(queue_it->second.m_type->name(), nullptr, nullptr, &status);

      throw QueueTypeMismatch(ERS_HERE, name, realname_source, realname_target);
    }

    return queuePtr;
  }

  auto config_it = this->m_queue_config_map.find(name);
  if (config_it != m_queue_config_map.end()) {
    QueueEntry entry = { &typeid(T), create_queue<T>(name, config_it->second) };
    m_queue_registry[name] = entry;
    return std::dynamic_pointer_cast<Queue<T>>(entry.m_instance);

  } else {
    // TODO: John Freeman (jcfree@fnal.gov), Jun-23-2020. Add checks for demangling status. Timescale 2 weeks.
    int status = -999;
    std::string realname_target = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status);
    throw QueueNotFound(ERS_HERE, name, realname_target);
  }
}

template<typename T>
std::shared_ptr<QueueBase>
QueueRegistry::create_queue(const std::string& name, const QueueConfig& config)
{

  std::shared_ptr<QueueBase> queue;
  switch (config.kind) {
    case QueueConfig::kStdDeQueue:
      queue = std::make_shared<StdDeQueue<T>>(name, config.capacity);
      break;
    case QueueConfig::kFollySPSCQueue:
      queue = std::make_shared<FollySPSCQueue<T>>(name, config.capacity);
      break;
    case QueueConfig::kFollyMPMCQueue:
      queue = std::make_shared<FollyMPMCQueue<T>>(name, config.capacity);
      break;

    default:
      throw QueueKindUnknown(ERS_HERE, std::to_string(config.kind));
  }

  return queue;
}

} // namespace dunedaq::iomanager
