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

  const confmodel::Queue* qptr = nullptr;
  for(auto& qcfg : m_queue_configs) {
    if (qcfg->UID() == name) {
      qptr = qcfg;
      break;
    }
  }
  if (qptr != nullptr) {
    QueueEntry entry = { qptr, &typeid(T), create_queue<T>(qptr) };
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
QueueRegistry::create_queue(const confmodel::Queue* config)
{
  std::shared_ptr<QueueBase> queue;
  auto type = config->get_queue_type();
  if (type == confmodel::Queue::Queue_type::KStdDeQueue) {
    queue = std::make_shared<StdDeQueue<T>>(config->UID(), config->get_capacity());
  } else if (type == confmodel::Queue::Queue_type::KFollySPSCQueue) {
    queue = std::make_shared<FollySPSCQueue<T>>(config->UID(), config->get_capacity());
  } else if (type == confmodel::Queue::Queue_type::KFollyMPMCQueue) {
    queue = std::make_shared<FollyMPMCQueue<T>>(config->UID(), config->get_capacity());
  } else {
    throw QueueTypeUnknown(ERS_HERE, config->get_queue_type());
  }

  m_opmon_link->register_node(config->UID(), queue);

  return queue;
}

} // namespace dunedaq::iomanager
