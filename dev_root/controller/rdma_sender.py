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
import math

from control import Control
from common import PacketSize


class RDMASender(Control):

    def __init__(self, target, gc, bfrt_info):
        # Set up base class
        super(RDMASender, self).__init__(target, gc)

        self.log = logging.getLogger(__name__)

        self.tables = [
            bfrt_info.table_get('pipe.Egress.rdma_sender.switch_mac_and_ip'),
            bfrt_info.table_get('pipe.Egress.rdma_sender.create_roce_packet'),
            bfrt_info.table_get('pipe.Egress.rdma_sender.fill_in_qpn_and_psn')
        ]

        self.switch_mac_and_ip = self.tables[0]
        self.create_roce_packet = self.tables[1]
        self.fill_in_qpn_and_psn = self.tables[2]

        # Annotations
        self.switch_mac_and_ip.info.data_field_annotation_add(
            'switch_mac', 'Egress.rdma_sender.set_switch_mac_and_ip', 'mac')
        self.switch_mac_and_ip.info.data_field_annotation_add(
            'switch_ip', 'Egress.rdma_sender.set_switch_mac_and_ip', 'ipv4')
        self.create_roce_packet.info.data_field_annotation_add(
            'dest_mac', 'Egress.rdma_sender.fill_in_roce_fields', 'mac')
        self.create_roce_packet.info.data_field_annotation_add(
            'dest_ip', 'Egress.rdma_sender.fill_in_roce_fields', 'ipv4')
        self.create_roce_packet.info.data_field_annotation_add(
            'dest_mac', 'Egress.rdma_sender.fill_in_roce_write_fields', 'mac')
        self.create_roce_packet.info.data_field_annotation_add(
            'dest_ip', 'Egress.rdma_sender.fill_in_roce_write_fields', 'ipv4')

        # Clear tables and reset counters
        self._clear()

    def set_switch_mac_and_ip(self, switch_mac, switch_ip):
        ''' Set switch MAC and IP '''

        # Clear table
        self.switch_mac_and_ip.default_entry_reset(self.target)

        self.switch_mac_and_ip.default_entry_set(
            self.target,
            self.switch_mac_and_ip.make_data([
                self.gc.DataTuple('switch_mac', switch_mac),
                self.gc.DataTuple('switch_ip', switch_ip)
            ], 'Egress.rdma_sender.set_switch_mac_and_ip'))

    def clear_rdma_workers(self):
        ''' Remove all RDMA workers '''
        self.create_roce_packet.entry_del(self.target)
        self.fill_in_qpn_and_psn.entry_del(self.target)

    def reset_counters(self):
        ''' Reset sent RDMA packets counters '''

        # Reset direct counter
        self.create_roce_packet.operations_execute(self.target, 'SyncCounters')
        resp = self.create_roce_packet.entry_get(self.target,
                                                 flags={'from_hw': False})

        keys = []
        values = []

        for v, k in resp:
            keys.append(k)

            v = v.to_dict()
            k = k.to_dict()

            values.append(
                self.create_roce_packet.make_data([
                    self.gc.DataTuple('$COUNTER_SPEC_BYTES', 0),
                    self.gc.DataTuple('$COUNTER_SPEC_PKTS', 0)
                ], v['action_name']))

        self.create_roce_packet.entry_mod(self.target, keys, values)

    def add_rdma_worker(self, worker_id, worker_mac, worker_ip, rkey,
                        packet_size, message_size, qpns_and_psns):
        ''' Add SwitchML RDMA entry.

            Keyword arguments:
                worker_id -- worker rank
                worker_mac -- worker MAC address
                worker_ip -- worker IP address
                rkey -- worker remote key
                packet_size -- MTU for this session
                message_size -- RDMA message size in bytes
                qpns_and_psns -- list of (QPn, initial psn) tuples

            Returns:
                (success flag, None or error message)
        '''

        # Add entry to fill in headers for RoCE packet
        self.create_roce_packet.entry_add(self.target, [
            self.create_roce_packet.make_key(
                [self.gc.KeyTuple('eg_md.switchml_md.worker_id', worker_id)])
        ], [
            self.create_roce_packet.make_data([
                self.gc.DataTuple('dest_mac', worker_mac),
                self.gc.DataTuple('dest_ip', worker_ip),
                self.gc.DataTuple('rkey', rkey)
            ], 'Egress.rdma_sender.fill_in_roce_write_fields')
        ])

        if packet_size == PacketSize.MTU_128:
            packet_size = 128
        elif packet_size == PacketSize.MTU_256:
            packet_size = 256
        elif packet_size == PacketSize.MTU_512:
            packet_size = 512
        elif packet_size == PacketSize.MTU_1024:
            packet_size = 1024

        packets_per_message = int(message_size / packet_size)

        if (packets_per_message &
            (packets_per_message - 1) != 0) or packets_per_message == 0:
            error_msg = 'Number of packets per message ({}) is not a power of 2'.format(
                packets_per_message)
            self.log.error(error_msg)
            return (False, error_msg)

        self.log.debug(
            'RDMA message size {}B, packet size {}B, packets per message {}'.
            format(message_size, packet_size, packets_per_message))

        # Add entry to add QPN and PSN to packet
        # Each QPN handles both sets of a slot in the pool.
        # Packets belonging to the same message use consecutive indices.
        for index, (qpn, initial_psn) in enumerate(qpns_and_psns):

            shifted_index = index * packets_per_message * 2
            mask = 0x7ffe & ~((packets_per_message - 1) << 1)

            self.fill_in_qpn_and_psn.entry_add(self.target, [
                self.fill_in_qpn_and_psn.make_key([
                    self.gc.KeyTuple('eg_md.switchml_md.worker_id', worker_id),
                    self.gc.KeyTuple('eg_md.switchml_md.pool_index',
                                     shifted_index, mask)
                ])
            ], [
                self.fill_in_qpn_and_psn.make_data([
                    self.gc.DataTuple('qpn', qpn),
                    self.gc.DataTuple('Egress.rdma_sender.psn_register.f1',
                                      initial_psn)
                ], 'Egress.rdma_sender.add_qpn_and_psn')
            ])

            self.log.debug(
                'Added QP {} with psn {} for index {:x} mask {:x}'.format(
                    qpn, initial_psn, shifted_index, mask))

        return (True, None)

    def get_workers_counter(self, worker_id=None):
        ''' Get the current values of sent packets/bytes per RDMA worker.
            If a worker ID is provided, it will return only the values for that
            worker, otherwise it will return all of them.
        '''

        self.create_roce_packet.operations_execute(self.target, 'SyncCounters')
        resp = self.create_roce_packet.entry_get(self.target,
                                                 flags={'from_hw': False})

        values = {}
        for v, k in resp:
            v = v.to_dict()
            k = k.to_dict()

            id = k['eg_md.switchml_md.worker_id']['value']
            mac = v['dest_mac']
            ip = v['dest_ip']
            packets = v['$COUNTER_SPEC_PKTS']
            bytes = v['$COUNTER_SPEC_BYTES']

            if worker_id == None or worker_id == id:
                values[id] = {
                    'MAC': mac,
                    'IP': ip,
                    'spkts': packets,
                    'sbytes': bytes
                }
        return values
