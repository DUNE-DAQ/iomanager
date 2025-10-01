/**
 * @file Queue.hpp
 *
 * This is the interface for Queue objects which connect DAQModules. Queues
 * are exposed to DAQModules via the DAQSource and DAQSink classes, and should
 * not be handled directly. Queues are registered with QueueRegistry for
 * retrieval by DAQSink and DAQSource instances.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_QUEUE_QUEUE_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_QUEUE_QUEUE_HPP_

#include "iomanager/queue/QueueBase.hpp"
#include "iomanager/queue/QueueIssues.hpp"

#include "ers/Issue.hpp"
#include "logging/Logging.hpp" // NOTE: if ISSUES ARE DECLARED BEFORE include logging/Logging.hpp, TLOG_DEBUG<<issue wont work.

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq::iomanager {

/**
 * @brief Implementations of the Queue class are responsible for relaying data
 * between DAQModules within a DAQ Application
 */
template<class T>
class Queue : public QueueBase
{
public:
  using value_t = T;                            ///< Type stored in the Queue
  using duration_t = std::chrono::milliseconds; ///< Base duration type for timeouts

  /**
   * @brief Queue Constructor
   * @param name Name of the Queue instance
   */
  explicit Queue(const std::string& name)
    : QueueBase(name)
  {
  }

  /**
   * @brief Determine whether the Queue may be pushed onto
   * @return True if the queue is not full, false if it is
   *
   */
  virtual bool can_push() const { return this->get_num_elements() < this->get_capacity(); }

  /**
   * @brief Determine whether the Queue may be popped from
   * @return True if the queue is not empty, false if it is
   *
   */
  virtual bool can_pop() const { return this->get_num_elements() > 0; }

  /**
   * @brief Push a value onto the Queue.
   * @param val Value to push (rvalue)
   * @param timeout Timeout for the push operation.
   *
   * This is a pure virtual function.
   * If push takes longer than the timeout, implementations should throw an
   * exception.
   */
  virtual void push(value_t&& val, const duration_t& timeout) = 0;

  /**
   * @brief Pop the first value off of the queue
   * @param val Reference to the value that is popped from the queue
   * @param timeout Timeout for the pop operation
   *
   * This is a pure virtual function
   * If pop takes longer than the timeout, implementations should throw an
   * exception
   */
  virtual void pop(value_t& val, const duration_t& timeout) = 0;

  virtual bool try_push(value_t&& val, const duration_t& timeout) = 0;
  virtual bool try_pop(value_t& val, const duration_t& timeout) = 0;

  std::function<void(T&&)> get_callback() { return m_callback; }
  void set_callback(std::function<void(T&&)> callback) { m_callback = callback; }

private:
  std::function<void(T&&)> m_callback;
  Queue(const Queue&) = delete;
  Queue& operator=(const Queue&) = delete;
  Queue(Queue&&) = delete;
  Queue& operator=(Queue&&) = delete;
};

} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_QUEUE_QUEUE_HPP_
