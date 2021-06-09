/**
 * SwitchML Project
 * @file rdma_grpc_client.cc
 * @brief Implements the RdmaGrpcClient class.
 */


#include "rdma_grpc_client.h"

#include "common_cc.h"
#include "switchml.grpc.pb.h"

namespace switchml {

// Print functions

std::ostream& operator<<(std::ostream& s, const SwitchML::BarrierRequest& r) {
    s << "<BarrierRequest" << std::hex << " job_size=0x" << r.job_size() << ">"
      << std::dec;
    return s;
}

std::ostream& operator<<(std::ostream& s,
                           const SwitchML::BarrierResponse& r) {
    s << "<BarrierResponse>";
    return s;
}

std::ostream& operator<<(std::ostream& s, const SwitchML::BcastRequest& r) {
  s << "<BcastRequest" << std::hex << " value=0x" << r.value()
    << " my_rank=0x" << r.my_rank() << " job_size=0x" << r.job_size()
    << " root_rank=0x" << r.root_rank() << ">" << std::dec;
  return s;
}

std::ostream& operator<<(std::ostream& s, const SwitchML::BcastResponse& r) {
  s << "<BcastResponse" << std::hex << " value=0x" << r.value() << ">"
    << std::dec;
  return s;
}

std::ostream& operator<<(std::ostream& s,
                           const SwitchML::RDMAConnectRequest& r) {
    s << "<RDMAConnectRequest" << std::hex << " job_id=" << r.job_id()
      << " rank=" << r.my_rank() << " size=" << r.job_size() << " mac=0x"
      << r.mac() << " ipv4=0x" << r.ipv4() << " rkey=0x" << r.rkey()
      << " packet_size=0x" << r.packet_size() << " message_size=0x"
      << r.message_size();

    for (int i = 0; i < r.qpns_size(); ++i) {
      s << " qpn=0x" << r.qpns(i) << " psn=0x" << r.psns(i);
    }
    return s << ">" << std::dec;
}

std::ostream& operator<<(std::ostream& s,
                           const SwitchML::RDMAConnectResponse& r) {
    s << "<RDMAConnectResponse" << std::hex << " job_id=" << r.job_id();

    for (int i = 0; i < r.macs_size(); ++i) {
      s << " mac=0x" << r.macs(i) << " ipv4=0x" << r.ipv4s(i) << " rkey=0x"
        << r.rkeys(i);
    }
    for (int i = 0; i < r.qpns_size(); ++i) {
      s << " qpn=0x" << r.qpns(i) << " psn=0x" << r.psns(i);
    }
    return s << ">" << std::dec;
}


// RdmaGrpcClient implementation

RdmaGrpcClient::RdmaGrpcClient(Config& config)
      : channel_(),
        rdmaserver_stub_(),
        synch_stub_() {
    std::string controller_socket = config.backend_.rdma.controller_ip_str + ":" + std::to_string(config.backend_.rdma.controller_port);
    this->channel_ = grpc::CreateChannel(controller_socket, grpc::InsecureChannelCredentials());
    this->rdmaserver_stub_ = SwitchML::RDMAServer::NewStub(this->channel_);
    this->synch_stub_ = SwitchML::Synch::NewStub(this->channel_);
}

RdmaGrpcClient::~RdmaGrpcClient() {}

void RdmaGrpcClient::RDMAConnect(const SwitchML::RDMAConnectRequest& request, SwitchML::RDMAConnectResponse* response) {
    DVLOG(1) << "Sending " << request;
    grpc::ClientContext context;
    grpc::Status status =
        rdmaserver_stub_->RDMAConnect(&context, request, response);
    CHECK(status.ok()) << "Error contacting coordinator: "
                       << status.error_code() << ": " << status.error_message();
    DVLOG(1) << "Received " << *response;
}

void RdmaGrpcClient::Bcast(const SwitchML::BcastRequest& request,
                         SwitchML::BcastResponse* response) {
    DVLOG(1) << "Sending " << request;
    grpc::ClientContext context;
    grpc::Status status = synch_stub_->Bcast(&context, request, response);
    CHECK(status.ok()) << "Error contacting coordinator: "
                       << status.error_code() << ": " << status.error_message();
    DVLOG(1) << "Received " << *response;
}

void RdmaGrpcClient::Barrier(const SwitchML::BarrierRequest& request,
                           SwitchML::BarrierResponse* response) {
    DVLOG(1) << "Sending " << request;
    grpc::ClientContext context;
    grpc::Status status = this->synch_stub_->Barrier(&context, request, response);
    CHECK(status.ok()) << "Error contacting coordinator: "
                       << status.error_code() << ": " << status.error_message();
    DVLOG(1) << "Received " << *response;
}

}  // namespace switchml
