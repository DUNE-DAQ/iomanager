#include "iomanager/queue/FollyQueue.hpp"
#include "iomanager/queue/StdDeQueue.hpp"

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

  auto config_it = this->m_queue_configs.begin();
  while (config_it != m_queue_configs.end()) {
    if (config_it->name == name)
      break;
    ++config_it;
  }
  if (config_it != m_queue_configs.end()) {
    QueueEntry entry = { *config_it, &typeid(T), create_queue<T>(*config_it) };
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
std::shared_ptr<Queue<T>>
QueueRegistry::get_queue(const ConnectionRequest& request)
{
  auto queue_it = m_queue_registry.begin();
  while (queue_it != m_queue_registry.end()) {
    if (is_match(request, queue_it->second.m_config)) {
      break;
    }
    ++queue_it;
  }
  if (queue_it != m_queue_registry.end()) {
    auto queuePtr = std::dynamic_pointer_cast<Queue<T>>(queue_it->second.m_instance);

    if (!queuePtr) {
      // TODO: John Freeman (jcfree@fnal.gov), Jun-23-2020. Add checks for demangling status. Timescale 2 weeks.
      int status = -999;
      std::string realname_target = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status);
      std::string realname_source = abi::__cxa_demangle(queue_it->second.m_type->name(), nullptr, nullptr, &status);

      throw QueueTypeMismatch(ERS_HERE, to_string(request), realname_source, realname_target);
    }

    return queuePtr;
  }

  auto config_it = this->m_queue_configs.begin();
  while (config_it != m_queue_configs.end()) {
    if (is_match(request, *config_it))
      break;
    ++config_it;
  }
  if (config_it != m_queue_configs.end()) {
    QueueEntry entry = { *config_it, &typeid(T), create_queue<T>(*config_it) };
    m_queue_registry[config_it->name] = entry;
    return std::dynamic_pointer_cast<Queue<T>>(entry.m_instance);

  } else {
    // TODO: John Freeman (jcfree@fnal.gov), Jun-23-2020. Add checks for demangling status. Timescale 2 weeks.
    int status = -999;
    std::string realname_target = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status);
    throw QueueNotFound(ERS_HERE, to_string(request), realname_target);
  }
}

template<typename T>
std::shared_ptr<QueueBase>
QueueRegistry::create_queue(const QueueConfig& config)
{
  std::shared_ptr<QueueBase> queue;
  switch (config.queue_type) {
    case QueueType::kStdDeQueue:
      queue = std::make_shared<StdDeQueue<T>>(config.name, config.capacity);
      break;
    case QueueType::kFollySPSCQueue:
      queue = std::make_shared<FollySPSCQueue<T>>(config.name, config.capacity);
      break;
    case QueueType::kFollyMPMCQueue:
      queue = std::make_shared<FollyMPMCQueue<T>>(config.name, config.capacity);
      break;

    default:
      throw QueueTypeUnknown(ERS_HERE, str(config.queue_type));
  }

  return queue;
}

} // namespace dunedaq::iomanager
