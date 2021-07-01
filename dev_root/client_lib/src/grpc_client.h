/**
 * SwitchML Project
 * @file grpc_client.h
 * @brief Declares the GrpcClient class.
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
 * @brief The GRPC client is the mediator between the client library and the controller program
 *
 * It can ask the controller to setup switch registers appropriately, and perform simple
 * collective communication operations across all workers (Currently only a barrier and a single value broadcast). 
 */
class GrpcClient {
// SUGESSTION: Is it more design conforming to have a seperate client for each backend?
  public:

    /**
     * @brief Create stubs and the grpc channel.
     * 
     * @param [in] config a reference to the switchml configuration.
     */
    GrpcClient(Config& config);

    ~GrpcClient() = default;

    GrpcClient(GrpcClient const&) = delete;
    void operator=(GrpcClient const&) = delete;

    GrpcClient(GrpcClient&&) = default;
    GrpcClient& operator=(GrpcClient&&) = default;

    /**
     * @brief A barrier across workers.
     * 
     * @param [in] request BarrierRequest containing the number of workers.
     * @param [out] response The empty BarrierResponse from the switch.
     */
    void Barrier(const switchml_proto::BarrierRequest& request,
                 switchml_proto::BarrierResponse* response);

    /**
     * @brief Broadcast a value to all workers through the controller.
     * 
     * @param [in] request 
     * @param [out] response 
     */
    void Broadcast(const switchml_proto::BroadcastRequest& request,
                   switchml_proto::BroadcastResponse* response);  

#ifdef RDMA
    /**
     * @brief Tell the controller to setup the switch registers for RDMA operation.
     * 
     * @param [in] request RdmaSessionRequest containing configuration, session info, memory region info
     * @param [out] response RdmaSessionResponse containing the switch's memory region info
     */
    void CreateRdmaSession(const switchml_proto::RdmaSessionRequest& request,
                           switchml_proto::RdmaSessionResponse* response);
#endif

#ifdef DPDK
    /**
     * @brief Tell the controller to setup the switch registers for UDP operation.
     * 
     * @param [in] request UdpSessionRequest containing configuration, session info
     * @param [out] response UdpSessionResponse
     */
    void CreateUdpSession(const switchml_proto::UdpSessionRequest& request,
                          switchml_proto::UdpSessionResponse* response);
#endif

  private:
    /** The grpc channel used by all calls */
    std::shared_ptr<grpc::Channel> channel_;

    /** This stub is used to perform the rdma session request call */
    std::unique_ptr<switchml_proto::Session::Stub> session_stub_;

    /** This stub is used to perform the barrier call */
    std::unique_ptr<switchml_proto::Sync::Stub> sync_stub_;
};

} // namespace switchml

#endif // SWITCHML_GRPC_CLIENT_H_