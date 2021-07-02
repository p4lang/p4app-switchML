/*
  Copyright 2021 Intel-KAUST-Microsoft

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

/**
 * SwitchML Project
 * @file rdma_timeout_queue.h
 * @brief Declares the TimeoutQueue class.
 */

#ifndef SWITCHML_RDMA_TIMEOUT_QUEUE_H_
#define SWITCHML_RDMA_TIMEOUT_QUEUE_H_

#include <chrono>
#include <vector>

#include "common.h"

namespace switchml {

/**
 * @brief An efficient data structure used to check for message timeouts.
 * 
 * In order to ensure that all operations are done in constant time,
 * the timeoutqueue was designed using an ordered double-linked list which also
 * has an index for entries. This allows us to perform all of the 3 functions (push, remove, check)
 * in constant time.
 */
class TimeoutQueue {
  public:
    /** Type of timestamp. Just an alias for user convenience. */
    using TimePoint = std::chrono::time_point<switchml::clock>;

    /**
     * @brief A struct representing a single entry in the TimeoutQueue.
     */
    struct TQEntry {
        TQEntry();

        ~TQEntry() = default;

        TQEntry(TQEntry const&) = default;
        void operator=(TQEntry const&) = delete;

        TQEntry(TQEntry&&) = default;
        TQEntry& operator=(TQEntry&&) = default;

        /** Whether this entry is just a place holder or an actual entry pushed by the user. */
        bool valid;
        /** The index of the next entry */
        int next;
        /** The index of the previous entry */
        int previous;
        /** The time at which this entry was pushed */
        TimePoint timestamp;
    };
    
    /**
     * @brief Construct a new Timeout Queue object
     * 
     * @param [in] num_entries The maximum number of entries that you might push.
     * This should equal the number of outstanding messages.
     * @param [in] timeout The initial value of the timeout in milliseconds.
     * @param [in] threshold After how many timeouts should we double the timeout value?.
     * @param [in] threshold After a timeout occurs how much should we increment the threshold?.
     */
    TimeoutQueue(const uint32_t num_entries,
                 const std::chrono::milliseconds timeout,
                 const uint32_t timeouts_threshold,
                 const uint32_t timeouts_threshold_increment);
    
    ~TimeoutQueue() = default;

    TimeoutQueue(TimeoutQueue const&) = default;
    void operator=(TimeoutQueue const&) = delete;

    TimeoutQueue(TimeoutQueue&&) = default;
    TimeoutQueue& operator=(TimeoutQueue&&) = default;

    /**
     * @brief Push an entry onto the queue.
     * 
     * Elements are added to the top of the linked list because they are always assumed to be the newest.
     * This allows us to keep the order and insert into the linked list in constant time.
     * 
     * @param [in] index The index where you want to store the entry for direct access later.
     * This is not the same as the entry's position in the linked list.
     * @param [in] timestamp The current timestamp
     */
    void Push(int index, const TimePoint& timestamp);

    /**
     * @brief Remove an entry.
     * 
     * This operation is done in constant time since the index
     * gives us direct access to the linked list element and we only
     * then need to rewire the pointers of the two adjacent entries in the linked list
     * if they exist.
     * 
     * @param [in] index The index of the entry to remove.
     */
    void Remove(int index);

    /**
     * @brief Given the current timestamp, check a timeout occured.
     * 
     * If a timeout occured, then the index of the entry that timed out first is returned.
     * 
     * This is also done in constant time since we only need to check the entry that exists
     * at the tail of the linked list.
     * 
     * @param [in] timestamp The current timestamp.
     * @return int the index of the entry that timed out first.
     */
    int Check(const TimePoint& timestamp);

  private:
    /** A vector that stores all of the linked list entries for direct access using an index */
    std::vector<TQEntry> entries_;

    /** The index of the head/top entry of the linked list */
    int head_;

    /** The index of the tail/bottom entry of the linked list */
    int tail_;

    /** 
     * After how much elapsed time do we consider an entry to have timed out?
     * This keeps doubling each time the timeouts_counter reaches the timeouts
     * threshold as a backoff mechanism.
     */
    std::chrono::milliseconds timeout_;

    /** 
     * A counter to keep track of how many timeouts occured since the last
     * doubling of the timeout value
     */
    uint32_t timeouts_counter_;

    /** After how many timeouts should we double the timeout value? */
    uint32_t timeouts_threshold_;

    /** After how many timeouts should we double the timeout value? */
    uint32_t timeouts_threshold_increment_;
};

} // namespace switchml

#endif // SWITCHML_RDMA_TIMEOUT_QUEUE_H_
