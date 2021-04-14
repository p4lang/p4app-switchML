/**
 * SwitchML Project
 * @file dummy_worker_thread.h
 * @brief Declares the DummyWorkerThread class.
 */

#ifndef SWITCHML_DUMMY_WORKER_THREAD_H_
#define SWITCHML_DUMMY_WORKER_THREAD_H_

#include "common.h"
#include "config.h"
#include "dummy_backend.h"

namespace switchml {

/**
 * @brief A class that represents a single dummy worker thread.
 * 
 * Multiple instances of this class is typically created depending on
 * the number of cores in the configuration.
 */
class DummyWorkerThread{
  public:
    /**
     * @brief Construct a new Dummy Worker Thread object
     * 
     * @param [in] context a reference to the switchml context.
     * @param [in] backend a reference to the created dummy backend.
     * @param [in] config a reference to the context configuration.
     */
    DummyWorkerThread(Context& context, DummyBackend& backend, Config& config);

    ~DummyWorkerThread();

    DummyWorkerThread(DummyWorkerThread const&) = delete;
    void operator=(DummyWorkerThread const&) = delete;

    DummyWorkerThread(DummyWorkerThread&&) = default;
    DummyWorkerThread& operator=(DummyWorkerThread&&) = default;

    /**
     * @brief This is the point of entry function for the thread.
     */
    void operator()();

    /** Worker thread id */
    const WorkerTid tid_;
  private:
    /** Monotonically increasing counter to give unique IDs for each new worker thread **/
    static WorkerTid next_tid_;
    /** A reference to the context */
    Context& context_;
    /** A reference to the context backend */
    DummyBackend& backend_;
    /** A reference to the context configuration */
    Config& config_;
};

} // namespace switchml
#endif // SWITCHML_WORKER_THREAD_H_