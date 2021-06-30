import logging

from control import Control
from common import PacketSize, PacketType
from bfrt_grpc.client import BfruntimeRpcException
from enum import IntEnum


class Flag(IntEnum):
    ''' First-last flag '''
    FIRST = 0x0
    LAST = 0x1


class NextStepSelector(Control):

    def __init__(self, target, gc, bfrt_info):
        # Set up base class
        super(NextStepSelector, self).__init__(target, gc)

        self.log = logging.getLogger(__name__)

        self.tables = [
            bfrt_info.table_get('pipe.Ingress.next_step_selector.next_step')
        ]
        self.table = self.tables[0]

        # Counters
        self.broadcast_counter = bfrt_info.table_get(
            'pipe.Ingress.next_step_selector.broadcast_counter')
        self.retransmit_counter = bfrt_info.table_get(
            'pipe.Ingress.next_step_selector.retransmit_counter')
        self.recirculate_counter = bfrt_info.table_get(
            'pipe.Ingress.next_step_selector.recirculate_counter')
        self.drop_counter = bfrt_info.table_get(
            'pipe.Ingress.next_step_selector.drop_counter')

        # Clear table and counters and add defaults
        self._clear()
        self.add_default_entries()

    def _clear(self):
        super(NextStepSelector, self)._clear()
        self.reset_counters()

    def add_default_entries(self):
        ''' Add default entries '''

        # Special recirculation ports used for harvest passes
        port = {1: 452, 2: 324, 3: 448, 4: 196, 5: 192, 6: 68, 7: 64}

        # yapf: disable
        entries = [
            # packet_size | worker_id | packet_type | first_last_flag | bitmap_result | priority | action | port |

            # 128B packets not supported at the moment
            ( PacketSize.MTU_128, None,                None,       None, None, 0, 'drop', None),

            # 256B packets: Pipe 0 only at the moment
            # Last packet -> recirculate for harvest
            ( PacketSize.MTU_256, None, PacketType.CONSUME0,  Flag.LAST, None, 1, 'recirculate_for_HARVEST7', port[7]),
            ## Just consume a CONSUME packet if it is not the last and we haven't seen it before
            ( PacketSize.MTU_256, None, PacketType.CONSUME0,       None,    0, 2, 'finish_consume', None),
            ## CONSUME packets that are retransmitted packets to a full slot -> recirculate for harvest
            ( PacketSize.MTU_256, None, PacketType.CONSUME0, Flag.FIRST, None, 3, 'recirculate_for_HARVEST7', port[7]),
            ## Drop others
            ( PacketSize.MTU_256, None, PacketType.CONSUME0,       None, None, 4, 'drop', None),

            ## 512B packets not supported at the moment
            ( PacketSize.MTU_512, None,                None,       None, None, 5, 'drop', None)]

        ## 1024B packets
        entries.extend([
            ## Pipe 0, packet not seen before -> pipe 1 (load balance on worker id)
            (PacketSize.MTU_1024,    i, PacketType.CONSUME0,       None,    0, 6, 'recirculate_for_CONSUME1', (1 << 7) + i * 4)
            for i in range(16)])
        entries.extend([
            ## Pipe 0, retransmitted packet to a full slot -> pipe 1
            ## Run through the same path as novel packets (do not skip to harvest) to ensure ordering
            (PacketSize.MTU_1024,    i, PacketType.CONSUME0, Flag.FIRST, None, 7, 'recirculate_for_CONSUME1', (1 << 7) + i * 4)
            for i in range(16)])
        entries.extend([
            ## Drop other CONSUME0 packets
            (PacketSize.MTU_1024, None, PacketType.CONSUME0,       None, None, 8, 'drop', None),
            ## Pipe 1 -> pipe 2
            (PacketSize.MTU_1024, None, PacketType.CONSUME1,       None, None, 9, 'recirculate_for_CONSUME2_same_port_next_pipe', None),
            ## Pipe 2 -> pipe 3
            (PacketSize.MTU_1024, None, PacketType.CONSUME2,       None, None, 10, 'recirculate_for_CONSUME3_same_port_next_pipe', None),
            ## Pipe 4
            ## For CONSUME3 packets that are the last packet, recirculate for harvest
            ## The last pass is a combined consume/harvest pass, so skip directly to HARVEST1
            (PacketSize.MTU_1024, None, PacketType.CONSUME3,  Flag.LAST, None, 11, 'recirculate_for_HARVEST1', port[1]),
            ## Just consume any CONSUME3 packets if they're not last and we haven't seen them before
            (PacketSize.MTU_1024, None, PacketType.CONSUME3,       None,    0, 12, 'finish_consume', None),
            ## CONSUME3 packets that are retransmitted packets to a full slot -> recirculate for harvest
            (PacketSize.MTU_1024, None, PacketType.CONSUME3, Flag.FIRST, None, 13, 'recirculate_for_HARVEST1', port[1]),
            ## Drop others
            (PacketSize.MTU_1024, None, PacketType.CONSUME3,       None, None, 14, 'drop', None),
            ## Harvesting 128B at a time
            (PacketSize.MTU_1024, None, PacketType.HARVEST0,       None, None, 15, 'recirculate_for_HARVEST1', port[1]),
            (PacketSize.MTU_1024, None, PacketType.HARVEST1,       None, None, 16, 'recirculate_for_HARVEST2', port[2]),
            (PacketSize.MTU_1024, None, PacketType.HARVEST2,       None, None, 17, 'recirculate_for_HARVEST3', port[3]),
            (PacketSize.MTU_1024, None, PacketType.HARVEST3,       None, None, 18, 'recirculate_for_HARVEST4', port[4]),
            (PacketSize.MTU_1024, None, PacketType.HARVEST4,       None, None, 19, 'recirculate_for_HARVEST5', port[5]),
            (PacketSize.MTU_1024, None, PacketType.HARVEST5,       None, None, 20, 'recirculate_for_HARVEST6', port[6]),
            (PacketSize.MTU_1024, None, PacketType.HARVEST6,       None, None, 21, 'recirculate_for_HARVEST7', port[7]),
            ## Harvest pass 7: final pass
            ## Read final 128B and broadcast any HARVEST packets that are not
            ## retransmitted and are the last packet
            (               None, None, PacketType.HARVEST7,  Flag.LAST,    0, 22, 'broadcast', None),
            ## First packet, not a retranmsmission
            ## (shouldn't ever get here, because the packet would be dropped in CONSUME)
            (               None, None, PacketType.HARVEST7, Flag.FIRST,    0, 23, 'drop', None),
            ## Retransmit any other HARVEST packets
            (               None, None, PacketType.HARVEST7, Flag.FIRST, None, 24, 'retransmit', None),
            ## Drop any other HARVEST packets
            (               None, None, PacketType.HARVEST7,       None, None, 25, 'drop', None)
        ])
        # yapf: enable

        for e in entries:
            success, error_msg = self.add_entry(
                **{
                    'packet_size': e[0],
                    'worker_id': e[1],
                    'packet_type': e[2],
                    'first_last_flag': e[3],
                    'bitmap_result': e[4],
                    'priority': e[5],
                    'action': e[6],
                    'recirc_dev_port': e[7]
                })

            if not success:
                self.log.critical(error_msg)

    def add_entry(self,
                  action,
                  recirc_dev_port=None,
                  packet_size=None,
                  worker_id=None,
                  packet_type=None,
                  first_last_flag=None,
                  bitmap_result=None,
                  priority=0):
        ''' Add next step selector entry. Match arguments can be None, in which case
            their mask will be zeroed.

            Keyword arguments:
                action -- action name
                recirc_dev_port -- recirculation port to use for the next pass (if the action requires it)
                packet_size -- packet MTU
                worker_id -- worker rank, or 2-tuple with (rank,mask)
                packet_type -- packet type
                first_last_flag -- first or last packet: 1: last 0: first
                bitmap_result -- result of bitmap check: 0: not a retransmission, not zero: retransmission
                priority -- entry priority (default: 0 = highest)

            Returns:
                (success flag, None or error message)
        '''

        # Parameters validation
        if packet_size != None and type(packet_size) != PacketSize:
            error_msg = 'Invalid packet size {}'.format(packet_size)
            self.log.error(error_msg)
            return (False, error_msg)

        if worker_id != None and worker_id >= 32:
            error_msg = 'Worker ID {} too large; only 32 workers supported'.format(
                worker_id)
            self.log.error(error_msg)
            return (False, error_msg)

        if packet_type != None and type(packet_type) != PacketType:
            error_msg = 'Invalid packet type {}'.format(packet_type)
            self.log.error(error_msg)
            return (False, error_msg)

        if first_last_flag != None and type(first_last_flag) != Flag:
            error_msg = 'Invalid first_last_flag type {}'.format(
                first_last_flag)
            self.log.error(error_msg)
            return (False, error_msg)

        actions_with_argument = [
            'recirculate_for_CONSUME1', 'recirculate_for_HARVEST1',
            'recirculate_for_HARVEST2', 'recirculate_for_HARVEST3',
            'recirculate_for_HARVEST4', 'recirculate_for_HARVEST5',
            'recirculate_for_HARVEST6', 'recirculate_for_HARVEST7'
        ]
        actions_without_argument = [
            'recirculate_for_CONSUME2_same_port_next_pipe',
            'recirculate_for_CONSUME3_same_port_next_pipe', 'finish_consume',
            'broadcast', 'retransmit', 'drop'
        ]
        action_prefix = 'Ingress.next_step_selector.'

        if action in actions_with_argument:
            if recirc_dev_port == None:
                error_msg = 'Missing recirculation port for action {}'.format(
                    action)
                self.log.error(error_msg)
                return (False, error_msg)
            else:
                data = self.table.make_data([
                    self.gc.DataTuple('recirc_port', recirc_dev_port),
                ], action_prefix + action)

        elif action in actions_without_argument:
            data = self.table.make_data([], action_prefix + action)
        else:
            error_msg = 'Invalid action {}'.format(action)
            self.log.error(error_msg)
            return (False, error_msg)

        # Convert parameters to 2-tuple (value,mask)
        packet_size = (0, 0) if packet_size == None else (packet_size,
                                                          0x7)  # 3 bits
        packet_type = (0, 0) if packet_type == None else (packet_type,
                                                          0xf)  # 4 bits
        first_last_flag = (0,
                           0) if first_last_flag == None else (first_last_flag,
                                                               0xff)  # 8 bits
        bitmap_result = (0, 0) if bitmap_result == None else (
            bitmap_result, 0xffffffff)  # 32 bits

        if worker_id == None:
            worker_id = (0, 0)
        elif type(worker_id) != tuple:
            worker_id = (worker_id, 0xffff)  # 16 bits

        # Add entry
        self.table.entry_add(self.target, [
            self.table.make_key([
                self.gc.KeyTuple('ig_md.switchml_md.packet_size',
                                 packet_size[0], packet_size[1]),
                self.gc.KeyTuple('ig_md.switchml_md.worker_id', worker_id[0],
                                 worker_id[1]),
                self.gc.KeyTuple('ig_md.switchml_md.packet_type',
                                 packet_type[0], packet_type[1]),
                self.gc.KeyTuple('ig_md.switchml_md.first_last_flag',
                                 first_last_flag[0], first_last_flag[1]),
                self.gc.KeyTuple('ig_md.switchml_md.map_result',
                                 bitmap_result[0], bitmap_result[1]),
                self.gc.KeyTuple('$MATCH_PRIORITY', priority)
            ])
        ], [data])

        self.log.debug('Next step entry: {}: packet_size {}, worker_id {}, '
                       'packet_type {}, first_last_flag {}, bitmap_result {}'
                       ' -> {}({})'.format(priority, packet_size, worker_id,
                                           packet_type, first_last_flag,
                                           bitmap_result, action,
                                           recirc_dev_port))

        return (True, None)

    def reset_counters(self):
        ''' Reset counters '''

        # Reset indirect counters
        self.broadcast_counter.entry_del(self.target)
        self.retransmit_counter.entry_del(self.target)
        self.recirculate_counter.entry_del(self.target)
        self.drop_counter.entry_del(self.target)

    def get_counters(self, start=0, count=None):
        ''' Get the current values of next step counters:
            broadcasted, retransmitted, recirculated, dropped packets per slot index.
            The parameters can limit the number of returned indices to
            [start, start + count]. The default is [0, 8].
        '''

        if count == None:
            count = 8

        # Double start and count to get both sets
        start = start * 2
        count = count * 2

        counters = {
            'broadcast': self.broadcast_counter,
            'retransmit': self.retransmit_counter,
            'recirculate': self.recirculate_counter,
            'drop': self.drop_counter
        }

        try:
            stats = {}
            for counter_name, counter in counters.items():
                counter.operations_execute(self.target, 'Sync')
                resp = counter.entry_get(self.target, [
                    counter.make_key([self.gc.KeyTuple('$COUNTER_INDEX', i)])
                    for i in range(start, start + count)
                ],
                                         flags={'from_hw': False})

                for v, k in resp:
                    v = v.to_dict()
                    k = k.to_dict()

                    pool_index = k['$COUNTER_INDEX']['value'] >> 1
                    pool_set = k['$COUNTER_INDEX']['value'] & 1
                    value = v['$COUNTER_SPEC_PKTS']

                    if ((pool_index, pool_set)) not in stats:
                        stats[pool_index, pool_set] = {}

                    stats[pool_index, pool_set][counter_name] = value

        except BfruntimeRpcException as bfrte:
            # Indices out of bound
            self.log.debug(str(bfrte))
            return []

        # Linearize
        values = []
        for (pool_index, pool_set), data in stats.items():
            data['index'] = pool_index
            data['set'] = pool_set
            values.append(data)

        return values
