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


class UDPSender(Control):

    def __init__(self, target, gc, bfrt_info):
        # Set up base class
        super(UDPSender, self).__init__(target, gc)

        self.log = logging.getLogger(__name__)

        self.tables = [
            bfrt_info.table_get('pipe.Egress.udp_sender.switch_mac_and_ip'),
            bfrt_info.table_get('pipe.Egress.udp_sender.dst_addr')
        ]

        self.switch_mac_and_ip = self.tables[0]
        self.dst_addr = self.tables[1]

        # Annotations
        self.switch_mac_and_ip.info.data_field_annotation_add(
            'switch_mac', 'Egress.udp_sender.set_switch_mac_and_ip', 'mac')
        self.switch_mac_and_ip.info.data_field_annotation_add(
            'switch_ip', 'Egress.udp_sender.set_switch_mac_and_ip', 'ipv4')
        self.dst_addr.info.data_field_annotation_add(
            'ip_dst_addr', 'Egress.udp_sender.set_dst_addr', 'ipv4')
        self.dst_addr.info.data_field_annotation_add(
            'eth_dst_addr', 'Egress.udp_sender.set_dst_addr', 'mac')

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
            ], 'Egress.udp_sender.set_switch_mac_and_ip'))

    def clear_udp_workers(self):
        ''' Remove all UDP workers '''
        self.dst_addr.entry_del(self.target)

    def reset_counters(self):
        ''' Reset sent UDP packets counters '''

        # Reset direct counter
        self.dst_addr.operations_execute(self.target, 'SyncCounters')
        resp = self.dst_addr.entry_get(self.target, flags={'from_hw': False})

        keys = []
        values = []

        for v, k in resp:
            keys.append(k)

            v = v.to_dict()
            k = k.to_dict()

            values.append(
                self.dst_addr.make_data([
                    self.gc.DataTuple('$COUNTER_SPEC_BYTES', 0),
                    self.gc.DataTuple('$COUNTER_SPEC_PKTS', 0)
                ], v['action_name']))

        self.dst_addr.entry_mod(self.target, keys, values)

    def add_udp_worker(self, worker_id, worker_mac, worker_ip):
        ''' Add SwitchML UDP entry.

            Keyword arguments:
                worker_id -- worker rank
                worker_mac -- worker MAC address
                worker_ip -- worker IP address
        '''
        self.dst_addr.entry_add(self.target, [
            self.dst_addr.make_key(
                [self.gc.KeyTuple('eg_md.switchml_md.worker_id', worker_id)])
        ], [
            self.dst_addr.make_data([
                self.gc.DataTuple('eth_dst_addr', worker_mac),
                self.gc.DataTuple('ip_dst_addr', worker_ip)
            ], 'Egress.udp_sender.set_dst_addr')
        ])

    def get_workers_counter(self, worker_id=None):
        ''' Get the current values of sent packets/bytes per UDP worker.
            If a worker ID is provided, it will return only the values for that
            worker, otherwise it will return all of them.
        '''

        self.dst_addr.operations_execute(self.target, 'SyncCounters')
        resp = self.dst_addr.entry_get(self.target, flags={'from_hw': False})

        values = {}
        for v, k in resp:
            v = v.to_dict()
            k = k.to_dict()

            id = k['eg_md.switchml_md.worker_id']['value']
            mac = v['eth_dst_addr']
            ip = v['ip_dst_addr']
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
