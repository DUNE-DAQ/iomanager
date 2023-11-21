#ifndef IOMANAGER_INCLUDE_IOMANAGER_FOLLYSPSCQUEUE_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_FOLLYSPSCQUEUE_HPP_

#include "folly/ProducerConsumerQueue.h"
#include "iomanager/queue/Queue.hpp"
#include <chrono>
#include "folly/portability/Asm.h"

namespace dunedaq {
namespace iomanager {

//
template<class T>
class FollyHPSPSCQueue : public Queue<T>
{
public:
  using value_t = T;
  using duration_t = typename Queue<T>::duration_t;

  explicit FollyHPSPSCQueue(const std::string& name, size_t capacity)
    : Queue<T>(name)
    , m_queue(capacity)
    , m_capacity(capacity)
  {
  }

  inline size_t get_capacity() const noexcept override { return m_capacity; }

  inline size_t get_num_elements() const noexcept override { return m_queue.sizeGuess(); }

  inline bool can_pop() const noexcept override { return !m_queue.isEmpty(); }

  inline void pop(value_t& val, const duration_t& timeout) override
  {

    if (!this->try_pop(val, timeout)) {
      throw QueueTimeoutExpired(ERS_HERE, this->get_name(), "pop", std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
    }
  }

  inline bool try_pop(value_t& val, const duration_t& timeout) override
  {

    // if (timeout > std::chrono::milliseconds::zero()) {
    //   auto start_time = std::chrono::steady_clock::now();
    //   auto time_wait_for_data_timeout = (start_time + timeout);
    //   // auto time_to_wait_for_data = (start_time + timeout) - std::chrono::steady_clock::now();
    //   // m_no_longer_empty.wait_for(lk, time_to_wait_for_data, [&]() { return this->can_pop(); });
    //   // Spin lock, baby
    //   while( std::chrono::steady_clock::now() < time_wait_for_data_timeout) {
    //     if (this->can_pop()) {
    //       break;
    //     }
    //     asm volatile("pause");
    //   }
    // }

    if (timeout > std::chrono::milliseconds::zero()) {
      if (!this->wait_for_no_longer_empty(timeout))
        return false;
    }

    return m_queue.read(val);
  }

  inline bool can_push() const noexcept override { return !m_queue.isFull(); }

  inline void push(value_t&& t, const duration_t& timeout) override
  {

    // if (!m_queue.write(std::move(t))) {
    if (!this->try_push(std::move(t), timeout)) {
      throw QueueTimeoutExpired(ERS_HERE, this->get_name(), "push", std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
    }
  }

  inline bool try_push(value_t&& t, const duration_t& timeout) override
  {

    // if (timeout > std::chrono::milliseconds::zero()) {
    //   auto start_time = std::chrono::steady_clock::now();
    //   auto time_wait_for_space_timeout = (start_time + timeout);

    //   // auto time_to_wait_for_space = (start_time + timeout) - std::chrono::steady_clock::now();
    //   // m_no_longer_full.wait_for(lk, time_to_wait_for_space, [&]() { return this->can_push(); });
    //   // Spin lock, baby
    //   while( std::chrono::steady_clock::now() < time_wait_for_space_timeout) {
    //     if (this->can_push()) {
    //       break;
    //     }
    //     asm volatile("pause");
    //   }
    // }
    
    if (timeout > std::chrono::milliseconds::zero()) {
      if (!this->wait_for_no_longer_full(timeout))
        return false;
    }

    if (!m_queue.write(std::move(t))) {
      ers::error(QueueTimeoutExpired(ERS_HERE, this->get_name(), "push", std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()));
      return false;
    }

    return true;
  }

  inline bool wait_for_no_longer_full(const duration_t& timeout) const
  {
    auto timeout_time = (std::chrono::steady_clock::now() + timeout);
    while (std::chrono::steady_clock::now() < timeout_time) {
      if (this->can_push()) {
        return true;
      }
      folly::asm_volatile_pause();
    }
    return false;
  }

  inline bool wait_for_no_longer_empty(const duration_t& timeout) const
  {
    auto timeout_time = (std::chrono::steady_clock::now() + timeout);
    while (std::chrono::steady_clock::now() < timeout_time) {
      if (this->can_pop()) {
        return true;
      }
      folly::asm_volatile_pause();
    }
    return false;
  }

  // Delete the copy and move operations
  FollyHPSPSCQueue(const FollyHPSPSCQueue&) = delete;
  FollyHPSPSCQueue& operator=(const FollyHPSPSCQueue&) = delete;
  FollyHPSPSCQueue(FollyHPSPSCQueue&&) = delete;
  FollyHPSPSCQueue& operator=(FollyHPSPSCQueue&&) = delete;

private:
  // The boolean argument is `MayBlock`, where "block" appears to mean
  // "make a system call". With `MayBlock` set to false, the queue
  // just spin-waits, so we want true
  folly::ProducerConsumerQueue<T> m_queue;
  size_t m_capacity;

  // std::condition_variable m_no_longer_full;
  // std::condition_variable m_no_longer_empty;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_FOLLYSPSCQUEUE_HPP_
