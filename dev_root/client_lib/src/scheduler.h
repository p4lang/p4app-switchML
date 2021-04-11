/**
 * SwitchML Project
 * @file scheduler.h
 * @brief Declares the Scheduler interface.
 */

#ifndef SWITCHML_SCHEDULER_H_
#define SWITCHML_SCHEDULER_H_

#include <mutex>
#include <memory>

#include "common.h"
#include "job.h"
#include "config.h"

namespace switchml {

/**
 * @brief The scheduler class which is responsible for distributing jobs across worker threads.
 * 
 * The scheduler implementation can choose any algorithm, queuing design, data structure,
 * or distribution mechanism to serve its purpose.
 * 
 * The scheduler should only be accessed through the context api.
 * Any scheduler implementation must be thread safe in the sense that it locks the scheduler
 * access lock before any function and releases it before exiting.
 * 
 * If more fine grained locking is needed then the implementation can create its own locks.
 */
class Scheduler {
  public:
    /**
     * @brief Creates a Scheduler object based on the scheduler name in the config
     * 
     * @param config a reference to the context configuration.
     * @return std::unique_ptr<Scheduler> an exclusive pointer to the created scheduler object.
     */
    static std::unique_ptr<Scheduler> CreateInstance(Config& config);

    ~Scheduler() = default;

    Scheduler(Scheduler const&) = delete;
    void operator=(Scheduler const&) = delete;

    Scheduler(Scheduler&&) = default;
    Scheduler& operator=(Scheduler&&) = default;


    /**
     * @brief Add a job to the Scheduler's queue.
     * 
     * This function is called by the context after a user submits a new communication job.
     * @param [in] job a shared pointer for the job that we will enqueue
     * @return true if we could add the request successfully.
     * @return false otherwise.
     */
    virtual bool EnqueueJob(std::shared_ptr<Job> job) = 0;

    /**
     * @brief Get a job request slice.
     * 
     * This is called through the context by worker threads to get a job slice.
     * How the Job is sliced and distributed depends on the scheduler implementation.
     * the function will block the calling thread on the job_submitted_event_ until a job slice is retrieved
     * OR the Stop is called. This is why it is important to check for the return value to make
     * sure that a job slice has been received.
     * 
     * @param [in] worker_thread_id  The id of the worker thread that wants a job slice.
     * @param [out] job_slice  A reference to a job slice variable. 
     * @return true if the scheduler returned a valid job slice.
     * @return false the caller was forced to wakeup and the scheduler did not return a valid job slice.
     */
    virtual bool GetJobSlice(WorkerTid worker_thread_id, JobSlice& job_slice) = 0;

    /**
     * @brief Signal the scheduler that a job slice has been finished.
     * 
     * Since the scheduler is the one responsible for creating job slices out of jobs,
     * it is the only entity that can know when a job is completed.
     * 
     * @param [in] worker_thread_id The id of the worker thread that finished the job slice.
     * @param [in] job_slice The job slice that finished.
     * @return true If the job corresponding to this job slice has been fully completed.
     * @return false If there is still some job slices to be completed either by the calling worker thread or others.
     */
    virtual bool NotifyJobSliceCompletion(WorkerTid worker_thread_id, const JobSlice& job_slice) = 0;

    /**
     * @brief Set the stopped_ flag to true and notify threads waiting on the job submitted event.
     * 
     * Each implementation should work to wakeup any waiting threads 
     * whether waiting on jobs or the scheduler itself. It should also clear any dynamically
     * allocated state.
     */
    virtual void Stop();

  protected:
    /**
     * @brief Construct a new Scheduler object
     * 
     * @param [in] config A reference to the context configuration
     */
    Scheduler(Config& config);

    /** A flag that signifies that the scheduler has been stopped_ */
    volatile bool stopped_;

    /** A mutex that is used to wrap all functions of the scheduler to make them thread safe. */
    std::mutex access_mutex_;

    /** A condition variable used by GetJobSlice to block until a job is available. */
    std::condition_variable job_submitted_event_;

    /** A reference to the context configuration */
    Config& config_;
};

} // namespace switchml
#endif // SWITCHML_SCHEDULER_H_