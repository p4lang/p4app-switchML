#ifndef _ARP_ICMP_RESPONDER_
#define _ARP_ICMP_RESPONDER_

#include "configuration.p4"
#include "types.p4"
#include "headers.p4"

control ARPandICMPResponder(
    inout header_t hdr,
    inout ingress_metadata_t ig_md,
    in ingress_intrinsic_metadata_t ig_intr_md,
    in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {

    // send packet back immediately
    action send_back() {
        // we assume this runs in parallel with or after
        // UDPReceiver which will set packet type IGNORE, so
        // packet will be forwarded
        ig_tm_md.ucast_egress_port = ig_intr_md.ingress_port;
    }

    action send_arp_reply(mac_addr_t switch_mac, ipv4_addr_t switch_ip) {
        hdr.ethernet.dst_addr = hdr.arp_ipv4.src_hw_addr;
        hdr.ethernet.src_addr = switch_mac;

        hdr.arp.opcode = arp_opcode_t.REPLY;
        hdr.arp_ipv4.dst_hw_addr    = hdr.arp_ipv4.src_hw_addr;
        hdr.arp_ipv4.dst_proto_addr = hdr.arp_ipv4.src_proto_addr;
        hdr.arp_ipv4.src_hw_addr    = switch_mac;
        hdr.arp_ipv4.src_proto_addr = switch_ip;

        send_back();
    }

    action send_icmp_echo_reply(mac_addr_t switch_mac, ipv4_addr_t switch_ip) {
        hdr.ethernet.dst_addr = hdr.ethernet.src_addr;
        hdr.ethernet.src_addr = switch_mac;

        hdr.ipv4.dst_addr = hdr.ipv4.src_addr;
        hdr.ipv4.src_addr = switch_ip;

        hdr.icmp.msg_type = icmp_type_t.ECHO_REPLY;
        hdr.icmp.checksum = 0;

        send_back();
    }

    table arp_icmp {
        key = {
            hdr.arp_ipv4.isValid()      : exact;
            hdr.icmp.isValid()          : exact;
            hdr.arp.opcode              : ternary;
            hdr.arp_ipv4.dst_proto_addr : ternary;
            hdr.icmp.msg_type           : ternary;
            hdr.ipv4.dst_addr           : ternary;
        }
        actions = {
            send_arp_reply;
            send_icmp_echo_reply;
        }
        size = 2;
    }

    apply {
        arp_icmp.apply();
    }
}

#endif /* _ARP_ICMP_RESPONDER_ */
