/**
 * SwitchML Project
 * @file rdma_grpc_client.h
 * @brief Declares the RdmaGrpcClient class.
 */

#ifndef SWITCHML_GRPC_CLIENT_H_
#define SWITCHML_GRPC_CLIENT_H_

#include <grpc++/grpc++.h>
#include <memory>

#include "common.h"
#include "config.h"
#include "switchml.grpc.pb.h"

namespace switchml {

/**
 * @brief The RDMA GRPC client is responsible for communicating with the controller to perform various information exchanges.
 * 
 * It is needed to send the required memory region information and other configurations that the switch needs to be aware of
 * before hand. It is also used to implement simple coordination schemes or small data collective operations across workers.
 */
class RdmaGrpcClient {
  public:

    /**
     * @brief Create stubs and the grpc channel.
     * 
     * @param [in] config a reference to the switchml configuration.
     */
    RdmaGrpcClient(Config& config);

    ~RdmaGrpcClient();

    RdmaGrpcClient(RdmaGrpcClient const&) = delete;
    void operator=(RdmaGrpcClient const&) = delete;

    RdmaGrpcClient(RdmaGrpcClient&&) = default;
    RdmaGrpcClient& operator=(RdmaGrpcClient&&) = default;

    /**
     * @brief Connect with the controller and exchange needed information for operation.
     * 
     * This must be called before any of the other functions can be used.
     * 
     * @param [in] request RDMAConnectRequest containing configuration, job info, memory region info
     * @param [out] response RDMAConnectResponse containing the switch's memory region info
     */
    void RDMAConnect(const SwitchML::RDMAConnectRequest& request,
                     SwitchML::RDMAConnectResponse* response);

    /**
     * @brief Broadcast general information to the switch.
     * 
     * @param [in] request 
     * @param [out] response 
     */
    void Bcast(const SwitchML::BcastRequest& request,
               SwitchML::BcastResponse* response);  

    /**
     * @brief A simple barrier across workers.
     * 
     * @param [in] request BarrierRequest containing the number of workers.
     * @param [out] response The empty BarrierResponse from the switch.
     */
    void Barrier(const SwitchML::BarrierRequest& request,
                 SwitchML::BarrierResponse* response);

  private:
    /** The grpc channel used by all calls */
    std::shared_ptr<grpc::Channel> channel_;

    /** This stub is used to perform the rdma connect call */
    std::unique_ptr<SwitchML::RDMAServer::Stub> rdmaserver_stub_;

    /** This stub is used to perform the barrier call */
    std::unique_ptr<SwitchML::Synch::Stub> synch_stub_;
};

} // namespace switchml

#endif // SWITCHML_GRPC_CLIENT_H_