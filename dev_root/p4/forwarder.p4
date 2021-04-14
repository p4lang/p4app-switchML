#ifndef _FORWARDER_
#define _FORWARDER_

#include "configuration.p4"
#include "types.p4"
#include "headers.p4"

control Forwarder(
    in header_t hdr,
    inout ingress_metadata_t ig_md,
    in ingress_intrinsic_metadata_t ig_intr_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {

    action set_egress_port(bit<9> egress_port) {
        ig_tm_md.ucast_egress_port = egress_port;
        ig_tm_md.bypass_egress = 1w1;
        ig_dprsr_md.drop_ctl[0:0] = 0;

        ig_md.switchml_md.setInvalid();
        ig_md.switchml_rdma_md.setInvalid();
    }

    action flood(MulticastGroupId_t flood_mgid) {
        ig_tm_md.mcast_grp_a         = flood_mgid;
        //We use 0x8000 + dev_port as the RID and XID for the flood group
        ig_tm_md.level1_exclusion_id = 7w0b1000000 ++ ig_intr_md.ingress_port;
        ig_tm_md.bypass_egress = 1w1;
        ig_dprsr_md.drop_ctl[0:0] = 0;

        ig_md.switchml_md.setInvalid();
        ig_md.switchml_rdma_md.setInvalid();
    }

    table forward {
        key = {
            hdr.ethernet.dst_addr : exact;
        }
        actions = {
            set_egress_port;
            flood;
        }
        size = forwarding_table_size;
    }

    apply {
        forward.apply();
    }
}

#endif /* _FORWARDER_ */
