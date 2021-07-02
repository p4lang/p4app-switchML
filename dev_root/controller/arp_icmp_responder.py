#  Copyright 2021 Intel-KAUST-Microsoft
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

import logging

from control import Control


class ARPandICMPResponder(Control):

    def __init__(self, target, gc, bfrt_info):
        # Set up base class
        super(ARPandICMPResponder, self).__init__(target, gc)

        self.log = logging.getLogger(__name__)

        self.tables = [
            bfrt_info.table_get('pipe.Ingress.arp_icmp_responder.arp_icmp')
        ]
        self.table = self.tables[0]

        # Lowest possible priority for ternary match rules
        self.lowest_priority = 1 << 24

        # Annotations
        self.table.info.key_field_annotation_add('hdr.arp_ipv4.dst_proto_addr',
                                                 'ipv4')
        self.table.info.key_field_annotation_add('hdr.ipv4.dst_addr', 'ipv4')
        self.table.info.data_field_annotation_add(
            'switch_mac', 'Ingress.arp_icmp_responder.send_arp_reply', 'mac')
        self.table.info.data_field_annotation_add(
            'switch_ip', 'Ingress.arp_icmp_responder.send_arp_reply', 'ipv4')
        self.table.info.data_field_annotation_add(
            'switch_mac', 'Ingress.arp_icmp_responder.send_icmp_echo_reply',
            'mac')
        self.table.info.data_field_annotation_add(
            'switch_ip', 'Ingress.arp_icmp_responder.send_icmp_echo_reply',
            'ipv4')

        # Clear table
        self._clear()

    def set_switch_mac_and_ip(self, switch_mac, switch_ip):
        ''' Set switch MAC and IP '''

        # Clear table
        self._clear()

        # Add entry to reply to arp requests
        self.table.entry_add(
            self.target,
            [
                self.table.make_key([
                    self.gc.KeyTuple('$MATCH_PRIORITY',
                                     self.lowest_priority - 2),
                    self.gc.KeyTuple(
                        'hdr.arp_ipv4.$valid',  # 1 bit
                        0x1),
                    self.gc.KeyTuple(
                        'hdr.icmp.$valid',  # 1 bit
                        0x0),
                    self.gc.KeyTuple(
                        'hdr.arp.opcode',  # 16 bits
                        0x0001,  # ARP request
                        0xffff),
                    self.gc.KeyTuple(
                        'hdr.arp_ipv4.dst_proto_addr',  # ARP who-has IP
                        switch_ip,
                        0xffffffff),
                    self.gc.KeyTuple(
                        'hdr.icmp.msg_type',  # 8 bits
                        0x00,  # Ignore for arp requests
                        0x00),
                    self.gc.KeyTuple(
                        'hdr.ipv4.dst_addr',  # Ignore for arp requests
                        switch_ip,
                        0x00000000)
                ])
            ],
            [
                self.table.make_data([
                    self.gc.DataTuple('switch_mac', switch_mac),
                    self.gc.DataTuple('switch_ip', switch_ip)
                ], 'Ingress.arp_icmp_responder.send_arp_reply')
            ])

        # Add entry to reply to ICMP echo requests
        self.table.entry_add(
            self.target,
            [
                self.table.make_key([
                    self.gc.KeyTuple('$MATCH_PRIORITY',
                                     self.lowest_priority - 1),
                    self.gc.KeyTuple(
                        'hdr.arp_ipv4.$valid',  # 1 bit
                        0x0),
                    self.gc.KeyTuple(
                        'hdr.icmp.$valid',  # 1 bit
                        0x1),
                    self.gc.KeyTuple(
                        'hdr.arp.opcode',  # 16 bits
                        0x0000,  # Ignore for icmp requests
                        0x0000),
                    self.gc.KeyTuple(
                        'hdr.arp_ipv4.dst_proto_addr',  # Ignore for icmp requests
                        switch_ip,
                        0x00000000),
                    self.gc.KeyTuple(
                        'hdr.icmp.msg_type',  # 8 bits
                        0x08,  # ICMP echo requet
                        0xff),
                    self.gc.KeyTuple(
                        'hdr.ipv4.dst_addr',  # ICMP dest addr
                        switch_ip,
                        0xffffffff)
                ])
            ],
            [
                self.table.make_data([
                    self.gc.DataTuple('switch_mac', switch_mac),
                    self.gc.DataTuple('switch_ip', switch_ip)
                ], 'Ingress.arp_icmp_responder.send_icmp_echo_reply')
            ])
