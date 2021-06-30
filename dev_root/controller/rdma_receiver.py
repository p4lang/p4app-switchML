import logging

from control import Control
from common import RDMAOpcode, WorkerType, max_num_queue_pairs_per_worker


class RDMAReceiver(Control):

    def __init__(self, target, gc, bfrt_info):
        # Set up base class
        super(RDMAReceiver, self).__init__(target, gc)

        self.log = logging.getLogger(__name__)

        self.tables = [
            bfrt_info.table_get('pipe.Ingress.rdma_receiver.receive_roce')
        ]
        self.table = self.tables[0]

        self.switch_mac = None
        self.switch_ip = None

        # Annotations
        self.table.info.key_field_annotation_add('hdr.ipv4.dst_addr', 'ipv4')
        self.table.info.key_field_annotation_add('hdr.ipv4.src_addr', 'ipv4')

        # Counters
        self.packet_counter = bfrt_info.table_get(
            'pipe.Ingress.rdma_receiver.rdma_packet_counter')
        self.message_counter = bfrt_info.table_get(
            'pipe.Ingress.rdma_receiver.rdma_message_counter')
        self.sequence_violation_counter = bfrt_info.table_get(
            'pipe.Ingress.rdma_receiver.rdma_sequence_violation_counter')

        # Workers
        self.worker_ids = []

        # Clear table and counters
        self._clear()

    def _clear(self):
        super(RDMAReceiver, self)._clear()
        self.reset_counters()
        self.worker_ids = []

    def set_switch_mac_and_ip(self, switch_mac, switch_ip):
        ''' Set switch MAC and IP '''
        self.switch_mac = switch_mac
        self.switch_ip = switch_ip

    def reset_counters(self):
        ''' Reset RDMA counters '''

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

        # Reset indirect counters
        self.packet_counter.entry_del(self.target)
        self.message_counter.entry_del(self.target)
        self.sequence_violation_counter.entry_del(self.target)

    def add_rdma_worker(self, worker_id, worker_ip, session_partition_key,
                        session_packet_size, num_workers, session_mgid):
        ''' Add SwitchML RoCEv2 entry.

            Keyword arguments:
                worker_id -- worker rank
                worker_ip -- worker IP address
                session_partition_key -- RDMA partition key to support virtual fabrics
                session_packet_size -- packet size used in this session
                num_workers -- number of workers in this session
                session_mgid -- multicast group ID for this session

            Returns:
                (success flag, None or error message)
        '''

        if self.switch_mac == None or self.switch_ip == None:
            error_msg = 'No switch address'
            self.log.error(error_msg)
            return (False, error_msg)

        worker_mask = 1 << worker_id

        # add entry for each opcode for each worker
        for opcode, action in [(RDMAOpcode.UC_RDMA_WRITE_FIRST,
                                'Ingress.rdma_receiver.first_packet'),
                               (RDMAOpcode.UC_RDMA_WRITE_MIDDLE,
                                'Ingress.rdma_receiver.middle_packet'),
                               (RDMAOpcode.UC_RDMA_WRITE_LAST,
                                'Ingress.rdma_receiver.last_packet'),
                               (RDMAOpcode.UC_RDMA_WRITE_ONLY,
                                'Ingress.rdma_receiver.only_packet'),
                               (RDMAOpcode.UC_RDMA_WRITE_LAST_IMMEDIATE,
                                'Ingress.rdma_receiver.last_packet'),
                               (RDMAOpcode.UC_RDMA_WRITE_ONLY_IMMEDIATE,
                                'Ingress.rdma_receiver.only_packet')]:

            # Switch virtual queue-pairs
            # lower 16 bits: QP numbers for one worker
            # upper 8 bits: worker ID
            # most significant bit set to 1 to help debugging
            qpn_top_bits = 0x800000 | ((worker_id & 0xff) << 16)

            self.table.entry_add(
                self.target,
                [
                    self.table.make_key([
                        self.gc.KeyTuple('$MATCH_PRIORITY', 10),
                        self.gc.KeyTuple('hdr.ipv4.src_addr', worker_ip),
                        self.gc.KeyTuple('hdr.ipv4.dst_addr', self.switch_ip),
                        self.gc.KeyTuple('hdr.ib_bth.partition_key',
                                         session_partition_key),
                        self.gc.KeyTuple('hdr.ib_bth.opcode', opcode),
                        # match on top bits of QP to support multiple clients
                        # on the same machine (same IP, different worker ID)
                        self.gc.KeyTuple('hdr.ib_bth.dst_qp', qpn_top_bits,
                                         0xff0000)
                    ])
                ],
                [
                    self.table.make_data(
                        [
                            self.gc.DataTuple('mgid', session_mgid),
                            self.gc.DataTuple('worker_type', WorkerType.ROCEv2),
                            self.gc.DataTuple('worker_id', worker_id),
                            self.gc.DataTuple('num_workers', num_workers),
                            self.gc.DataTuple('worker_bitmap', worker_mask),
                            self.gc.DataTuple('packet_size',
                                              session_packet_size),
                            # Clear direct counter
                            self.gc.DataTuple('$COUNTER_SPEC_BYTES', 0),
                            self.gc.DataTuple('$COUNTER_SPEC_PKTS', 0)
                        ],
                        action)
                ])

        # Save worker id
        self.worker_ids.append(worker_id)
        return (True, None)

    def get_workers_counter(self, worker_id=None):
        ''' Get the current values of received packets/bytes per RDMA worker.
            If a worker ID is provided, it will return only the values for that
            worker, otherwise it will return all of them.
        '''

        self.table.operations_execute(self.target, 'SyncCounters')
        resp = self.table.entry_get(self.target, flags={'from_hw': False})

        values = {}
        for v, k in resp:
            v = v.to_dict()
            k = k.to_dict()

            ip = k['hdr.ipv4.src_addr']['value']
            id = v['worker_id']
            packets = v['$COUNTER_SPEC_PKTS']
            bytes = v['$COUNTER_SPEC_BYTES']

            if worker_id == None or worker_id == id:
                values[id] = {'IP': ip, 'rpkts': packets, 'rbytes': bytes}
        return values

    def get_queue_pairs_counters(self, start=0, count=None):
        ''' Get the current values of received packets and messages per queue pair.
            The parameters can limit the number of returned counters per worker to the
            indices [start, start + count].
        '''

        self.packet_counter.operations_execute(self.target, 'Sync')
        self.message_counter.operations_execute(self.target, 'Sync')
        self.sequence_violation_counter.operations_execute(self.target, 'Sync')

        if count == None:
            count = max_num_queue_pairs_per_worker

        ids = [
            worker_id * max_num_queue_pairs_per_worker + offset
            for worker_id in self.worker_ids
            for offset in range(start, start + count)
        ]
        ids.sort()

        if len(ids) == 0:
            return []

        packets_resp = self.packet_counter.entry_get(self.target, [
            self.packet_counter.make_key(
                [self.gc.KeyTuple('$COUNTER_INDEX', i)]) for i in ids
        ],
                                                     flags={'from_hw': False})

        messages_resp = self.message_counter.entry_get(self.target, [
            self.message_counter.make_key(
                [self.gc.KeyTuple('$COUNTER_INDEX', i)]) for i in ids
        ],
                                                       flags={'from_hw': False})

        sequence_violations_resp = self.sequence_violation_counter.entry_get(
            self.target, [
                self.sequence_violation_counter.make_key(
                    [self.gc.KeyTuple('$COUNTER_INDEX', i)]) for i in ids
            ],
            flags={'from_hw': False})

        stats = {}
        for v, k in packets_resp:
            v = v.to_dict()
            k = k.to_dict()

            idx = k['$COUNTER_INDEX']['value']
            packets = v['$COUNTER_SPEC_PKTS']
            worker_id = int(idx / max_num_queue_pairs_per_worker)
            queue_pair_number = idx % max_num_queue_pairs_per_worker
            stats[idx] = {
                'packets': packets,
                'worker_id': worker_id,
                'qpn': queue_pair_number
            }

        for v, k in messages_resp:
            v = v.to_dict()
            k = k.to_dict()

            idx = k['$COUNTER_INDEX']['value']
            try:
                stats[idx]['messages'] = v['$COUNTER_SPEC_PKTS']
            except KeyError:
                self.log.error(
                    'RDMA packet counter is missing the value for index {}')
                stats[idx]['messages'] = 'N/A'

        for v, k in sequence_violations_resp:
            v = v.to_dict()
            k = k.to_dict()

            idx = k['$COUNTER_INDEX']['value']
            try:
                stats[idx]['sequence_violations'] = v['$COUNTER_SPEC_PKTS']
            except KeyError:
                self.log.error(
                    'RDMA packet counter is missing the value for index {}')
                stats[idx]['sequence_violations'] = 'N/A'

        # Linearize
        values = []
        for id, data in stats.items():
            data['qp_index'] = id
            values.append(data)

        return values
