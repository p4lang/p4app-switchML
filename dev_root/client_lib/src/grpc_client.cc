/**
 * SwitchML Project
 * @file grpc_client.cc
 * @brief Implements the GrpcClient class.
 */


#include "grpc_client.h"

#include "common_cc.h"
#include "switchml.grpc.pb.h"

namespace switchml {

// Print functions

std::ostream& operator<<(std::ostream& s, const switchml_proto::BarrierRequest& r) {
    s << "<BarrierRequest" << std::hex << " num_workers=0x" << r.num_workers() << ">"
      << std::dec;
    return s;
}

std::ostream& operator<<(std::ostream& s, const switchml_proto::BarrierResponse& r) {
    s << "<BarrierResponse>";
    return s;
}

std::ostream& operator<<(std::ostream& s, const switchml_proto::BroadcastRequest& r) {
  s << "<BroadcastRequest" << std::hex << " value=0x" << r.value()
    << " rank=0x" << r.rank() << " num_workers=0x" << r.num_workers()
    << " root=0x" << r.root() << ">" << std::dec;
  return s;
}

std::ostream& operator<<(std::ostream& s, const switchml_proto::BroadcastResponse& r) {
  s << "<BcastResponse" << std::hex << " value=0x" << r.value() << ">"
    << std::dec;
  return s;
}

#ifdef RDMA
std::ostream& operator<<(std::ostream& s, const switchml_proto::RdmaSessionRequest& r) {
    s << "<RDMASessionRequest" << std::hex << " session_id=" << r.session_id()
      << " rank=" << r.rank() << " num_workers=" << r.num_workers() << " mac=0x"
      << r.mac() << " ipv4=0x" << r.ipv4() << " rkey=0x" << r.rkey()
      << " packet_size=0x" << r.packet_size() << " message_size=0x"
      << r.message_size();

    for (int i = 0; i < r.qpns_size(); ++i) {
      s << " qpn=0x" << r.qpns(i) << " psn=0x" << r.psns(i);
    }
    return s << ">" << std::dec;
}

std::ostream& operator<<(std::ostream& s, const switchml_proto::RdmaSessionResponse& r) {
    s << "<RDMASesssionResponse" << std::hex << " session_id=" << r.session_id() 
      << " mac=" << r.mac() << " ipv4=" << r.ipv4() << " rkey=" << r.rkey();

    for (int i = 0; i < r.qpns_size(); ++i) {
      s << " qpn=0x" << r.qpns(i) << " psn=0x" << r.psns(i);
    }
    return s << ">" << std::dec;
}
#endif

#ifdef DPDK
std::ostream& operator<<(std::ostream& s, const switchml_proto::UdpSessionRequest& r) {
    s << "<UDPSessionRequest" << std::hex << " session_id=" << r.session_id()
      << " rank=" << r.rank() << " num_workers=" << r.num_workers() << " mac=0x"
      << r.mac() << " ipv4=0x" << r.ipv4() << " packet_size=0x" << r.packet_size();
    return s << ">" << std::dec;
}

std::ostream& operator<<(std::ostream& s, const switchml_proto::UdpSessionResponse& r) {
    s << "<UDPSessionResponse" << std::hex << " session_id=" << r.session_id() 
      << " mac=" << r.mac() << " ipv4=" << r.ipv4();
    return s << ">" << std::dec;
}
#endif

// GrpcClient implementation

GrpcClient::GrpcClient(Config& config)
      : channel_(),
        session_stub_(),
        sync_stub_() {
    std::string controller_socket = config.general_.controller_ip_str + ":" + std::to_string(config.general_.controller_port);
    this->channel_ = grpc::CreateChannel(controller_socket, grpc::InsecureChannelCredentials());
    this->session_stub_ = switchml_proto::Session::NewStub(this->channel_);
    this->sync_stub_ = switchml_proto::Sync::NewStub(this->channel_);
}

void GrpcClient::Barrier(const switchml_proto::BarrierRequest& request,
                             switchml_proto::BarrierResponse* response) {
    DVLOG(1) << "Sending " << request;
    grpc::ClientContext context;
    grpc::Status status = this->sync_stub_->Barrier(&context, request, response);
    CHECK(status.ok()) << "Error contacting coordinator: "
                       << status.error_code() << ": " << status.error_message();
    DVLOG(1) << "Received " << *response;
}

void GrpcClient::Broadcast(const switchml_proto::BroadcastRequest& request,
                               switchml_proto::BroadcastResponse* response) {
    DVLOG(1) << "Sending " << request;
    grpc::ClientContext context;
    grpc::Status status = sync_stub_->Broadcast(&context, request, response);
    CHECK(status.ok()) << "Error contacting coordinator: "
                       << status.error_code() << ": " << status.error_message();
    DVLOG(1) << "Received " << *response;
}

#ifdef RDMA
void GrpcClient::CreateRdmaSession(const switchml_proto::RdmaSessionRequest& request,
                                       switchml_proto::RdmaSessionResponse* response) {
    DVLOG(1) << "Sending " << request;
    grpc::ClientContext context;
    grpc::Status status =
        this->session_stub_->RdmaSession(&context, request, response);
    CHECK(status.ok()) << "Error contacting coordinator: "
                       << status.error_code() << ": " << status.error_message();
    DVLOG(1) << "Received " << *response;
}
#endif

#ifdef DPDK
void GrpcClient::CreateUdpSession(const switchml_proto::UdpSessionRequest& request,
                                       switchml_proto::UdpSessionResponse* response) {
    DVLOG(1) << "Sending " << request;
    grpc::ClientContext context;
    grpc::Status status =
        this->session_stub_->UdpSession(&context, request, response);
    CHECK(status.ok()) << "Error contacting coordinator: "
                       << status.error_code() << ": " << status.error_message();
    DVLOG(1) << "Received " << *response;
}
#endif

}  // namespace switchml
