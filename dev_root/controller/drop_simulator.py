import logging

from control import Control


class DropSimulator(Control):

    def __init__(self, target, gc, bfrt_info):
        # Set up base class
        super(DropSimulator, self).__init__(target, gc)

        self.log = logging.getLogger(__name__)

        self.tables = [bfrt_info.table_get('pipe.IngressParser.$PORT_METADATA')]
        self.table = self.tables[0]

        # Counter
        self.simulated_drop_counter = bfrt_info.table_get(
            'pipe.Ingress.egress_drop_sim.simulated_drop_packet_counter')

        self.ingress_drop_probability = 0
        self.egress_drop_probability = 0

        # Clear table and counter (set the drop probability to zero)
        self._clear()

    def _clear(self):
        self.table.entry_del(self.target)
        self.reset_counter()

    def set_drop_probabilities(self, ingress_drop_probability,
                               egress_drop_probability):
        ''' Set the probabilities of simulated drops.

            Keyword arguments:
                ingress_drop_probability -- probability to drop in ingress
                egress_drop_probability -- probability to drop in egress

            Returns:
                (success flag, ingress_drop_probability or error message,
                egress_drop_probability or None)
        '''

        if not 0 <= ingress_drop_probability <= 1:
            return (False, 'Ingress drop probability must be in [0,1]', None)
        if not 0 <= egress_drop_probability <= 1:
            return (False, 'Egress drop probability must be in [0,1]', None)

        # Compute the actual value of probabilities
        ingress_drop_value = int(0xffff * ingress_drop_probability)
        self.ingress_drop_probability = float(ingress_drop_value) / 0xffff

        egress_drop_value = int(0xffff * egress_drop_probability)
        self.egress_drop_probability = float(egress_drop_value) / 0xffff

        self.log.info('Ingress drop probability: 0x{:0x} or {:.2f}'.format(
            ingress_drop_value, self.ingress_drop_probability))
        self.log.info('Egress drop probability: 0x{:0x} or {:.2f}'.format(
            egress_drop_value, self.egress_drop_probability))

        if ingress_drop_value == 0 and egress_drop_value == 0:
            self.table.entry_del(self.target)
        else:
            # Set in all ports
            num_ports = 288
            self.table.entry_add(self.target, [
                self.table.make_key(
                    [self.gc.KeyTuple('ig_intr_md.ingress_port', p)])
                for p in range(num_ports)
            ], [
                self.table.make_data([
                    self.gc.DataTuple('ingress_drop_probability',
                                      ingress_drop_value),
                    self.gc.DataTuple('egress_drop_probability',
                                      egress_drop_value)
                ])
            ] * num_ports)
        return (True, self.ingress_drop_probability,
                self.egress_drop_probability)

    def get_drop_probabilities(self):
        ''' Get the probabilities of simulated drops '''
        return self.ingress_drop_probability, self.egress_drop_probability

    def reset_counter(self):
        ''' Reset dropped packets counter '''

        self.simulated_drop_counter.entry_del(self.target)

    def get_counter(self, qpn=None):
        ''' Get the current values of dropped packets counter per-QP.
            If a queue pair number is provided, it will return only
            the value for that queue pair, otherwise it will return
            all of them. '''

        self.simulated_drop_counter.operations_execute(self.target, 'Sync')
        count = self.simulated_drop_counter.info.size

        resp = self.simulated_drop_counter.entry_get(self.target, [
            self.simulated_drop_counter.make_key(
                [self.gc.KeyTuple('$COUNTER_INDEX', i)]) for i in range(count)
        ],
                                                     flags={'from_hw': False})

        values = []
        for v, k in resp:
            v = v.to_dict()
            k = k.to_dict()

            idx = k['$COUNTER_INDEX']['value']
            pkts = v['$COUNTER_SPEC_PKTS']
            if qpn == None or qpn == idx:
                values.append({'QP': idx, 'packets': pkts})
        return values
