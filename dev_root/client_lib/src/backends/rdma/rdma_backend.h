/**
 * SwitchML Project
 * @file rdma_backend.h
 * @brief Declares the RdmaBackend class.
 */

#ifndef SWITCHML_RDMA_BACKEND_H_
#define SWITCHML_RDMA_BACKEND_H_

#ifndef RDMA
#define RDMA // Just so that IDEs activate the rdma sections of the code
#endif

#include <memory>

#include "common.h"
#include "backend.h"
#include "rdma_connection.h"

#define RDMA_SWITCH_ELEMENT_SIZE 4

namespace switchml {

// Forward declare RdmaWorkerThread so we can add it as a member variable
class RdmaWorkerThread;

/**
 * @brief The backend that represents the rdma version of switchml.
 */
class RdmaBackend : public switchml::Backend {
  public:

    /**
     * @brief Call the super class constructor.
     * 
     * @param [in] context The context
     * @param [in] config The context configuration.
     */
    RdmaBackend(Context& context, Config& config);

    ~RdmaBackend();

    RdmaBackend(RdmaBackend const&) = delete;
    void operator=(RdmaBackend const&) = delete;

    RdmaBackend(RdmaBackend&&) = default;
    RdmaBackend& operator=(RdmaBackend&&) = default;

    /**
     * @brief Establish the RDMA connection, setup the switch, and start the worker threads.
     */
    void SetupWorker() override;

    /**
     * @brief Wait for all worker threads to exit.
     */
    void CleanupWorker() override;

    /**
     * @brief Get the RDMA connection object that the worker threads will use to send and receive.
     * 
     * @return std::unique_ptr<RdmaConnection>& 
     */
    std::unique_ptr<RdmaConnection>& GetConnection();

  private:
    /** Stores all of the rdma worker threads */
    std::vector<RdmaWorkerThread> worker_threads_;

    /** The RDMA connection that the worker threads will use to send and receive */
    std::unique_ptr<RdmaConnection> connection_;
};

} // namespace switchml
#endif // SWITCHML_RDMA_BACKEND_H_