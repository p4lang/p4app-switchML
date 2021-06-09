/**
 * SwitchML Project
 * @file rdma_timeout_queue.cc
 * @brief Implements the TimeoutQueue class.
 */

#include "rdma_timeout_queue.h"

#include <glog/logging.h>

#include "common_cc.h"

namespace switchml {

TimeoutQueue::TQEntry::TQEntry()
    : valid(false),
      next(-1),
      previous(-1),
      timestamp() 
{
    // Do nothing
}

TimeoutQueue::TimeoutQueue(const unsigned int num_entries,
                             const std::chrono::milliseconds timeout,
                             const unsigned int threshold)
  : entries_(num_entries),
    head_(-1),
    tail_(-1),
    timeout_(timeout),
    timeouts_counter_(0),
    timeouts_threshold_(threshold) 
{
    // Do nothing
}

void TimeoutQueue::Push(int index, const TimePoint& timestamp) {
    // Fail if new insertion is older than current head
    LOG_IF(FATAL, this->head_ != -1 && timestamp < this->entries_[this->head_].timestamp)
        << "Inserting out-of-order timestamp for QP " << index;

    // Remove current entry for this index
    Remove(index);

    // Set up new entry at head of list
    this->entries_[index].valid = true;
    this->entries_[index].previous = -1;  // no previous link since this is newest
    this->entries_[index].next = this->head_;
    this->entries_[index].timestamp = timestamp;

    // Add back link to new entry
    if (this->head_ != -1) {
        this->entries_[this->head_].previous = index;
    }

    // Change head
    this->head_ = index;

    // If this is the only element, update tail index
    if (this->tail_ == -1) {
        this->tail_ = index;
    }
}

void TimeoutQueue::Remove(int index) {
    if (this->entries_[index].valid) {
        // If the entry has a previous link
        if (this->entries_[index].previous != -1) {
          // copy this entry's next link to its previous link
          this->entries_[this->entries_[index].previous].next = this->entries_[index].next;
        }

        // If the entry has a next link
        if (this->entries_[index].next != -1) {
          // copy this entry's next link to its previous link
          this->entries_[this->entries_[index].next].previous = this->entries_[index].previous;
        }

        // If this entry is at the head
        if (this->head_ == index) {
          // update the head with the next element
          this->head_ = this->entries_[index].next;
        }

        // If this entry is at the tail
        if (this->tail_ == index) {
          // update the tail with the previous element
          this->tail_ = this->entries_[index].previous;
        }

        // Invalidate entry
        this->entries_[index].next = -1;
        this->entries_[index].previous = -1;
        this->entries_[index].valid = false;
    }
}

int TimeoutQueue::Check(const TimePoint& timestamp) {
    if (this->tail_ != -1) {
        // Check if the oldest entry has triggered a timeout
        std::chrono::milliseconds interval =
            std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - this->entries_[this->tail_].timestamp);
        if(interval > this->timeout_) {
            // Timeout
            this->timeouts_counter_++;
            if (this->timeouts_counter_ > this->timeouts_threshold_) {
                // Backoff: we double the timeout once the timeouts exceed the threshold
                this->timeouts_counter_ = 0;
                this->timeout_ *= 2;
            }
            return this->tail_;
        }
    }
    // No timeout
    return -1;
}

}  // namespace switchml
