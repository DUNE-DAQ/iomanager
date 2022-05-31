
#include "ers/ers.hpp"

namespace dunedaq::iomanager {

template<class T>
StdDeQueue<T>::StdDeQueue(const std::string& name, size_t capacity)
  : Queue<T>(name)
  , m_deque()
  , m_capacity(capacity)
  , m_size(0)
{
  assert(m_deque.max_size() > this->get_capacity());
}

template<class T>
void
StdDeQueue<T>::push(value_t&& object_to_push, const duration_t& timeout)
{

  auto start_time = std::chrono::steady_clock::now();
  std::unique_lock<std::mutex> lk(m_mutex, std::defer_lock);

  this->try_lock_for(lk, timeout);

  auto time_to_wait_for_space = (start_time + timeout) - std::chrono::steady_clock::now();

  if (time_to_wait_for_space.count() > 0) {
    m_no_longer_full.wait_for(lk, time_to_wait_for_space, [&]() { return this->can_push(); });
  }

  if (this->can_push()) {
    m_deque.push_back(std::move(object_to_push));
    m_size++;
    m_no_longer_empty.notify_one();
  } else {
    throw QueueTimeoutExpired(
      ERS_HERE, this->get_name(), "push", std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
  }
}

template<class T>
void
StdDeQueue<T>::pop(T& val, const duration_t& timeout)
{

  auto start_time = std::chrono::steady_clock::now();
  std::unique_lock<std::mutex> lk(m_mutex, std::defer_lock);

  this->try_lock_for(lk, timeout);

  auto time_to_wait_for_data = (start_time + timeout) - std::chrono::steady_clock::now();

  if (time_to_wait_for_data.count() > 0) {
    m_no_longer_empty.wait_for(lk, time_to_wait_for_data, [&]() { return this->can_pop(); });
  }

  if (this->can_pop()) {
    val = std::move(m_deque.front());
    m_size--;
    m_deque.pop_front();
    m_no_longer_full.notify_one();
  } else {
    throw QueueTimeoutExpired(
      ERS_HERE, this->get_name(), "pop", std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
  }
}

template<class T>
bool
StdDeQueue<T>::push_noexcept(value_t&& object_to_push, const duration_t& timeout)
{

    auto start_time = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lk(m_mutex, std::defer_lock);

    this->try_lock_for(lk, timeout);

    auto time_to_wait_for_space = (start_time + timeout) - std::chrono::steady_clock::now();

    if (time_to_wait_for_space.count() > 0) {
        m_no_longer_full.wait_for(lk, time_to_wait_for_space, [&]() { return this->can_push(); });
    }

    if (this->can_push()) {
        m_deque.push_back(std::move(object_to_push));
        m_size++;
        m_no_longer_empty.notify_one();
        return true;
    }
    else {
        ers::error( QueueTimeoutExpired(
            ERS_HERE, this->get_name(), "push", std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()));
        return false;
    }
}

template<class T>
bool
StdDeQueue<T>::pop_noexcept(T& val, const duration_t& timeout)
{

    auto start_time = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lk(m_mutex, std::defer_lock);

    this->try_lock_for(lk, timeout);

    auto time_to_wait_for_data = (start_time + timeout) - std::chrono::steady_clock::now();

    if (time_to_wait_for_data.count() > 0) {
        m_no_longer_empty.wait_for(lk, time_to_wait_for_data, [&]() { return this->can_pop(); });
    }

    if (this->can_pop()) {
        val = std::move(m_deque.front());
        m_size--;
        m_deque.pop_front();
        m_no_longer_full.notify_one();
        return true;
    }
    else {
        ers::error( QueueTimeoutExpired(
            ERS_HERE, this->get_name(), "pop", std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()));
        return false;
    }
}

// This try_lock_for() function was written because while objects of
// type std::timed_mutex have their own try_lock_for functions, the
// std::condition_variable::wait_for functions used in this class's push
// and pop operations require an std::mutex

template<class T>
void
StdDeQueue<T>::try_lock_for(std::unique_lock<std::mutex>& lk, const duration_t& timeout)
{
  assert(!lk.owns_lock());

  auto start_time = std::chrono::steady_clock::now();
  lk.try_lock();

  if (!lk.owns_lock() && timeout.count() > 0) {

    int approximate_number_of_retries = 5;
    duration_t pause_between_tries = duration_t(timeout.count() / approximate_number_of_retries);

    while (std::chrono::steady_clock::now() < start_time + timeout) {
      std::this_thread::sleep_for(pause_between_tries);
      lk.try_lock();
      if (lk.owns_lock()) {
        break;
      }
    }
  }

  if (!lk.owns_lock()) {
    throw QueueTimeoutExpired(
      ERS_HERE, this->get_name(), "lock mutex", std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
  }
}

} // namespace dunedaq::iomanager
