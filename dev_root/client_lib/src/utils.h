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

/**
 * @brief A function to execute any command on the system and return the standard output as a string.
 * 
 * @param [in] cmd the command to execute.
 * @return std::string a string that contains the standard output of the command.
 */
inline std::string Execute(const char* cmd) {
    std::vector<char> buffer(128);
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        LOG(FATAL) << "popen() failed!";
        exit(1);
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

} // namespace switchml
#endif // SWITCHML_UTILS_H_
