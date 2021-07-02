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
 * @file context.h
 * @brief Declares the Context class or in other words the SwitchML API.
 * 
 * This should be the first file that a new developer examines.
 */

#ifndef SWITCHML_CONTEXT_H_
#define SWITCHML_CONTEXT_H_

#include <mutex>
#include <condition_variable>

#include "common.h"
#include "scheduler.h"
#include "config.h"
#include "backend.h"
#include "stats.h"
#include "job.h"

namespace switchml {

/**
 * @brief Singleton class that represents the SwitchML API.
 * 
 * This is the starting point for all SwitchML operations.
 * Simply create a context, start the context, do your operations, stop the context.
 */
class Context {
  public:
    /**
     * @brief An enum to describe the context's state.
     * 
     * The context goes through all states sequentially during its lifetime.
     */
    enum ContextState {
        CREATED,  /**< Was just constructed. must call Start(). */
        STARTING, /**< In the process of initializing and starting. */
        RUNNING,  /**< Running and ready to receive job requests. */
        STOPPING, /**< In the process of shutting down. */
        STOPPED,  /**< Shutdown completed. */
    };

    Context(Context const&) = delete; 
    void operator=(Context const&) = delete; 

    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;
    
    /**
     * @brief Gets a reference to the single Context object.
     * 
     * A new instance is created (Constructor is called) when you call this function for the first time.
     * Subsequent calls will retrieve the same context object.
     * The instance only gets destroyed (Destructor is called) when the program exits like the default with any static object.
     * 
     * @return Context& A reference to the context object.
     */
    static Context& GetInstance();

    /**
     * @brief Perform all needed initializations to make SwitchML ready to be used through the context api.
     * 
     * The function performs all of the following:
     *  - Parse configuration files
     *  - Initialize and allocate variables and structures.
     *  - Setup the backend (This includes starting worker threads)
     * 
     * @param [in] config A pointer to a configuration object to use. If the argument is not passed
     * then the configuration will be created and loaded from the default configuration paths
     * using Config::LoadFromFile().
     * @see Stop()
     * @return true Initialization was successfull and you can start using the context.
     * @return false Initialization failed. Any subsequent calls to the context api will have undefined behavior. 
     */
    bool Start(Config* config = NULL);

    /**
     * @brief Performs all needed steps to stop switchml and cleanup all of its state.
     * 
     * The function performs all of the following:
     *  - Clean up the backend (This includes stopping worker threads and **waiting** for them)
     *  - Clean up all dynamically allocated memory.
     * @see Start()
     */
    void Stop();

    /**
     * @brief The function will submit an all reduce Job to the Context Scheduler then return immedietly.
     * 
     * The reduced tensor will be stored inplace in the same buffer provided.
     * Consider calling WaitForCompletion or GetJobStatus on the returned Job object reference to make sure that it completed.
     * 
     * @param [in] in_ptr Pointer to the memory where to read data
     * @param [in] out_ptr Pointer to the memory where to write processed data (The results)
     * @param [in] numel Number of elements (Not size)
     * @param [in] data_type The type of the data (FLOAT32, INT32).
     * @param [in] all_reduce_operation what kind of all reduce operation do you want to perform?
     * @return std::shared_ptr<Job> A shared pointer to the job that was submitted.
     * @see AllReduce()
     */
    std::shared_ptr<Job> AllReduceAsync(void* in_ptr, void* out_ptr, uint64_t numel, DataType data_type, AllReduceOperation all_reduce_operation);

    /**
     * @brief Convenience function equivelant to calling AllReduceAsync then waiting on the returned job reference.
     * @see AllReduceAsync()
     * @see Job::WaitToComplete()
     */
    std::shared_ptr<Job> AllReduce(void* in_ptr, void* out_ptr, uint64_t numel, DataType data_type, AllReduceOperation all_reduce_operation);

    /**
     * @brief Blocks the calling thread until SwitchML finishes all submited work.
     * 
     * Finishing includes failing and dropping the job. So the job status should be checked.
     * @see Job::WaitToComplete()
     */
    void WaitForAllJobs();

    /**
     * @brief Get the current Context State.
     * 
     * @return ContextState 
     */
    ContextState GetContextState();

    /**
     * @brief Get a constant reference to the active configuration.
     * 
     * @return const Config&
     */
    const Config& GetConfig();

    /**
     * @brief Get a reference to the statistics object used.
     * 
     * @return Stats&
     */
    Stats& GetStats();

  private:
    /**
     * @brief Default initializes all members and puts the context in the CREATED state.
     * 
     * It also initializes the logging library and logs the client library's version info.
     * 
     * This does not start the context as the Start() function is responsible for that.
     * @see Start()
     */
    Context();

    /**
     * @brief Makes sure that the context is stopped if Stop() was not called explicitly.
     * 
     * @see Stop()
     */
    ~Context();

    /**
     * @brief Wrapper for the scheduler's GetJobSlice.
     * 
     * Added to avoid accessing the scheduler directly.
     * 
     * @param [in] worker_thread_id The id of the worker thread that wants a job slice.
     * @param [out] job_slice A reference to a job slice variable. 
     */
    bool GetJobSlice(WorkerTid worker_thread_id, JobSlice& job_slice);

    /**
     * @brief Wrapper for the scheduler's NotfiyJobSliceCompletion. 
     * 
     * Added to avoid accessing the scheduler directly.
     * And to give the context a chance to update its current jobs map.
     * 
     * @param [in] job_slice The job slice that finished.
     * @param [in] worker_thread_id The id of the worker thread that finished the job slice.
     */
    void NotifyJobSliceCompletion(WorkerTid worker_thread_id, const JobSlice& job_slice);

    // We want GetJobSlice, NotifyJobSliceCompletion, and get Backend to only be accessible to the worker thread and not the client.
    friend class DummyWorkerThread;
#ifdef DPDK
    friend class DpdkWorkerThread;
#endif
#ifdef RDMA
    friend class RdmaWorkerThread;
#endif

    /** The scheduler that will be used to dispatch job slices to worker threads. */
    std::unique_ptr<Scheduler> scheduler_;

    /** The backend that will be used for launching threads and doing communication */
    std::unique_ptr<Backend> backend_;

    /** The active SwitchML configuration */
    Config config_;

    /** A stats object to keep track of all stats in all parts of the library */
    Stats stats_; 

    /** An atomic variable of the Current context state. */
    std::atomic<ContextState> context_state_;

    /** The number of jobs submitted that haven't finished yet. */
    int number_of_current_jobs_;
    
    /** Mutex to protect access to object members and to be used by the all_jobs_finished_event */
    std::mutex access_mutex_;
    
    /** 
     * An event that signifies that all current jobs reached 0 and that all submitted jobs have finished. 
     * This is used by the WaitForAllJobs() function.
     */
    std::condition_variable all_jobs_finished_event_;
};

} // namespace switchml
#endif // SWITCHML_CONTEXT_H_