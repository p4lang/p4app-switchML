/**
 * SwitchML Project
 * @file utils.cc
 * @brief Implements various utility classes and functions.
 */

#include "utils.h"

#include "common.h"

namespace switchml {

Barrier::Barrier(const int num_participants)
    : num_participants_(num_participants)
    , access_mutex_()
    , condition_variable_()
    , count_(num_participants) // Initialize barrier to waiting state, expecting num_participants 
    , flag_(false) // initial value of flag doesn't matter; only a change in value matters
{
    // nothing to do here
}

Barrier::~Barrier() {
    this->Destroy();
}

void Barrier::Wait() {
    std::unique_lock<std::mutex> lock(access_mutex_);
    LOG_IF(FATAL, this->count_ == -1) << "Attempting to wait at barrier after it was destroyed";
    
    // grab a copy of the current flag value
    const bool flag_copy = this->flag_;
    
    // note this thread has arrived
    this->count_--;
    
    if (this->count_ > 0) { 
        // if this thread is not the last one to arrive, wait for the
        // flag to change, indicating that the last thread has arrived
        this->condition_variable_.wait(lock, [flag_copy, this] {
            return this->count_ == -1 || flag_copy != this->flag_;
        });
    } else {
        // if this thread is the last one to arrive, flip the flag,
        // reset the count for the next iteration, and notify waiters
        this->flag_ = !this->flag_;
        this->count_ = this->num_participants_;
        this->condition_variable_.notify_all();
    }
}

void Barrier::Destroy() {
    std::unique_lock<std::mutex> lock(access_mutex_);
    this->count_ = -1;
    this->condition_variable_.notify_all(); 
}

} // namespace switchml
