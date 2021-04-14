/**
 * SwitchML Project
 * @file utils.h
 * @brief Declares various utility classes and functions.
 */

#ifndef SWITCHML_UTILS_H_
#define SWITCHML_UTILS_H_

#include <mutex>
#include <condition_variable>

#include "common.h"

namespace switchml {

/**
 * @brief A class that implements a simple thread barrier.
 * Simply create an instance that is visible to using threads then from
 * each thread call the wait function.
 */
class Barrier {
  public:
    /**
     * @brief Construct a new Barrier object
     * 
     * @param [in] num_participants Number of threads that will use the barrier
     */
    Barrier(const int num_participants);

    /**
     * @brief Call Destroy() just in case it hasn't been called and some threads are waiting.
     * 
     * @see Destroy()
     */
    ~Barrier();

    Barrier(Barrier const&) = delete;
    void operator=(Barrier const&) = delete;

    Barrier(Barrier&&) = default;
    Barrier& operator=(Barrier&&) = default;

    /**
     * @brief Block the thread until all other participating threads arrive at the barrier.
     */
    void Wait();

    /**
     * @brief Wakeup all waiting threads and make this barrier unusable.
     */
    void Destroy();
    
  private:
    /** Number of threads that will use the barrier */
    const int num_participants_;
    /** The mutex used to protect the members. */
    std::mutex access_mutex_;
    /** The condition used to suspend and notify threads */
    std::condition_variable condition_variable_;
    /** Number of participants that still haven't arrived. */
    int count_;
    /** This flag is used to differentiate between adjacent barrier invocations to avoid deadlocks. */
    bool flag_;
};

} // namespace switchml
#endif // SWITCHML_UTILS_H_
