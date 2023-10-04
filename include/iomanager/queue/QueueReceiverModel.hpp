/**
 * @file Receiver.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_QRECEIVER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_QRECEIVER_HPP_

#include "iomanager/Receiver.hpp"
#include "iomanager/queue/Queue.hpp"

#include "logging/Logging.hpp"

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace dunedaq {
namespace iomanager {

// QImpl
template<typename Datatype>
class QueueReceiverModel : public ReceiverConcept<Datatype>
{
public:
  explicit QueueReceiverModel(ConnectionId const& request);

  QueueReceiverModel(QueueReceiverModel&& other);

  ~QueueReceiverModel() { remove_callback(); }

  Datatype receive(Receiver::timeout_t timeout) override;

  std::optional<Datatype> try_receive(Receiver::timeout_t timeout) override;

  void add_callback(std::function<void(Datatype&)> callback) override;

  void remove_callback() override;

  // Topics are not used for Queues
  void subscribe(std::string) override {}
  void unsubscribe(std::string) override {}

private:
  std::atomic<bool> m_with_callback{ false };
  std::function<void(Datatype&)> m_callback;
  std::unique_ptr<std::thread> m_event_loop_runner;
  std::shared_ptr<Queue<Datatype>> m_queue;
};

} // namespace iomanager
} // namespace dunedaq

#include "detail/QueueReceiverModel.hxx"

#endif // IOMANAGER_INCLUDE_IOMANAGER_RECEIVER_HPP_
