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
from common import WorkerType


class UDPReceiver(Control):

    def __init__(self, target, gc, bfrt_info):
        # Set up base class
        super(UDPReceiver, self).__init__(target, gc)

        self.log = logging.getLogger(__name__)

        self.tables = [
            bfrt_info.table_get('pipe.Ingress.udp_receiver.receive_udp')
        ]
        self.table = self.tables[0]

        self.switch_mac = None
        self.switch_ip = None

        # Annotations
        self.table.info.key_field_annotation_add('hdr.ethernet.dst_addr', 'mac')
        self.table.info.key_field_annotation_add('hdr.ethernet.src_addr', 'mac')
        self.table.info.key_field_annotation_add('hdr.ipv4.dst_addr', 'ipv4')
        self.table.info.key_field_annotation_add('hdr.ipv4.src_addr', 'ipv4')

        # Clear table
        self._clear()

    def _clear(self):
        super(UDPReceiver, self)._clear()
        self.reset_counters()

    def set_switch_mac_and_ip(self, switch_mac, switch_ip):
        ''' Set switch MAC and IP '''
        self.switch_mac = switch_mac
        self.switch_ip = switch_ip

    def reset_counters(self):
        ''' Reset UDP counters '''

        # Reset direct counter
        self.table.operations_execute(self.target, 'SyncCounters')
        resp = self.table.entry_get(self.target, flags={'from_hw': False})

        keys = []
        values = []

        for v, k in resp:
            keys.append(k)

            v = v.to_dict()
            k = k.to_dict()

            values.append(
                self.table.make_data([
                    self.gc.DataTuple('$COUNTER_SPEC_BYTES', 0),
                    self.gc.DataTuple('$COUNTER_SPEC_PKTS', 0)
                ], v['action_name']))

        self.table.entry_mod(self.target, keys, values)

    def add_udp_worker(self, worker_id, worker_mac, worker_ip, udp_port,
                       udp_mask, num_workers, session_mgid):
        ''' Add SwitchML UDP entry.
            One of the worker_mac and worker_ip arguments can be None,
            but not both.

            Keyword arguments:
                udp_port -- base UDP port for this session
                udp_mask -- mask for UDP port for this session
                worker_id -- worker rank
                worker_mac -- worker MAC address
                worker_ip -- worker IP address
                num_workers -- number of workers in this session
                session_mgid -- multicast group ID for this session

            Returns:
                (success flag, None or error message)
        '''
        if worker_mac == None and worker_ip == None:
            error_msg = 'Missing worker identifier for worker rank {}'.format(
                worker_id)
            self.log.error(error_msg)
            return (False, error_msg)

        if self.switch_mac == None or self.switch_ip == None:
            error_msg = 'No switch address'
            self.log.error(error_msg)
            return (False, error_msg)

        worker_mask = 1 << worker_id
        worker_ip_mask = '255.255.255.255'
        worker_mac_mask = 'FF:FF:FF:FF:FF:FF'
        match_priority = 10

        if worker_ip == None:
            worker_ip = worker_ip_mask = '0.0.0.0'
        if worker_mac == None:
            worker_mac = worker_mac_mask = '00:00:00:00:00:00'

        self.table.entry_add(
            self.target,
            [
                self.table.make_key([
                    self.gc.KeyTuple('$MATCH_PRIORITY', match_priority),
                    # Don't match on ingress port; accept packets from a particular
                    # worker no matter which port it comes in on.
                    self.gc.KeyTuple(
                        'ig_intr_md.ingress_port',
                        0x000,  # 9 bits
                        0x000),
                    # Match on Ethernet addrs, IPs and port
                    self.gc.KeyTuple('hdr.ethernet.src_addr', worker_mac,
                                     worker_mac_mask),
                    self.gc.KeyTuple('hdr.ethernet.dst_addr', self.switch_mac,
                                     'FF:FF:FF:FF:FF:FF'),
                    self.gc.KeyTuple('hdr.ipv4.src_addr', worker_ip,
                                     worker_ip_mask),
                    self.gc.KeyTuple('hdr.ipv4.dst_addr', self.switch_ip,
                                     '255.255.255.255'),
                    self.gc.KeyTuple('hdr.udp.dst_port', udp_port, udp_mask),
                    # Ignore parser errors
                    self.gc.KeyTuple('ig_prsr_md.parser_err', 0x0000, 0x0000)
                ])
            ],
            [
                self.table.make_data([
                    self.gc.DataTuple('mgid', session_mgid),
                    self.gc.DataTuple('worker_type', WorkerType.SWITCHML_UDP),
                    self.gc.DataTuple('worker_id', worker_id),
                    self.gc.DataTuple('num_workers', num_workers),
                    self.gc.DataTuple('worker_bitmap', worker_mask)
                ], 'Ingress.udp_receiver.set_bitmap')
            ])
        return (True, None)

    def get_workers_counter(self, worker_id=None):
        ''' Get the current values of received packets/bytes per UDP worker.
            If a worker ID is provided, it will return only the values for that
            worker, otherwise it will return all of them.
        '''

        self.table.operations_execute(self.target, 'SyncCounters')
        resp = self.table.entry_get(self.target, flags={'from_hw': False})

        values = {}
        for v, k in resp:
            v = v.to_dict()
            k = k.to_dict()

            mac = k['hdr.ethernet.src_addr']['value']
            ip = k['hdr.ipv4.src_addr']['value']
            id = v['worker_id']
            packets = v['$COUNTER_SPEC_PKTS']
            bytes = v['$COUNTER_SPEC_BYTES']

            if worker_id == None or worker_id == id:
                values[id] = {
                    'MAC': mac,
                    'IP': ip,
                    'rpkts': packets,
                    'rbytes': bytes
                }
        return values
