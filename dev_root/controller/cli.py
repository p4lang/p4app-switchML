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

import inspect
import logging
import readline

from cmd import Cmd
from colors import color  #ansicolors

from common import front_panel_regex, mac_address_regex, validate_ip


class CommandError(Exception):
    ''' Command failed '''
    pass


class Cli(Cmd, object):

    doc_header = 'Available commands (type help <cmd>):'
    ruler = '='

    def __init__(self, *args, **kwargs):
        super(Cli, self).__init__(*args, **kwargs)
        self.log = logging.getLogger(__name__)

    def setup(self,
              controller,
              prompt='',
              stdin=None,
              use_rawinput=None,
              name=''):

        self.ctrl = controller
        self.prompt = '{}>'.format(prompt)
        self.name = name

        if stdin is not None:
            self.stdin = stdin
        if use_rawinput is not None:
            self.use_rawinput = use_rawinput

        # Remove -:/ from word delimiters
        delims = readline.get_completer_delims()
        delims = delims.replace('-', '')
        delims = delims.replace(':', '')
        delims = delims.replace('/', '')
        readline.set_completer_delims(delims)

    def out(self, message, clr=None):
        ''' Print output message '''
        print(color('\n{}\n'.format(message), fg=clr))

    def doc(self, message):
        ''' Print command documentation '''
        self.out(message, 'yellow')

    def error(self, message):
        ''' Print error message '''
        self.out(message, 'red')

    def default(self, line):
        self.error('Unknown command: {}'.format(line))
        return self.do_help('')

    def emptyline(self):
        # Do nothing
        pass

    def do_help(self, arg):
        ''' List available commands with 'help' or detailed help with 'help <cmd>' '''
        if arg:
            try:
                # Get help message from docstring
                method = getattr(self, 'do_' + arg)
                doc_lines = inspect.getdoc(method).splitlines(True)
                if doc_lines:
                    # Insert command on top
                    doc_lines.insert(0, '{}\n'.format(arg))
                    self.doc('  '.join(doc_lines))
                    return
            except AttributeError:
                pass
        # Use default help method
        super(Cli, self).do_help(arg)

    def cmdloop(self, intro=None):

        self.out(self.name)

        while True:
            try:
                super(Cli, self).cmdloop(intro='')
                break
            except KeyboardInterrupt:
                self.doc('Type exit to exit from {}'.format(self.name))
            except Exception as e:
                self.log.exception(e)
                self.error('Unexpected error. See log for details')

    def run(self):
        self.cmdloop()

    def do_exit(self, line):
        ''' Quit the CLI '''

        self.out('Bye!')
        return True

    do_EOF = do_exit

    def do_history(self, line):
        ''' Show commands history '''

        if line:
            self.error('Unknown parameter: {}'.format(line.strip()))
        else:
            msg = ''
            for i in range(readline.get_current_history_length() - 1):
                msg += '{}: {}\n'.format(i, readline.get_history_item(i + 1))
            self.out(msg)

    # Commands
    def do_show_ports(self, line):
        ''' Show active ports. If a front-panel/lane is provided,
            only that port will be shown.
        '''

        port = None
        lane = None
        stats = None

        try:
            if line:
                re_match = front_panel_regex.match(line.strip())

                if re_match and re_match.group(1):
                    port = int(re_match.group(1))
                    if not (1 <= port and port <= 64):
                        raise CommandError('Port number invalid')
                else:
                    raise CommandError('Port number invalid')

                if re_match.group(2):
                    lane = int(re_match.group(2))
                    if lane not in range(0, 4):
                        raise CommandError('Invalid lane')
                else:
                    lane = 0

            success, stats = self.ctrl.ports.get_stats(port, lane)
            if not success:
                raise CommandError(stats)

        except CommandError as e:
            self.error(e)
            return

        format_string = (
            '  {$PORT_NAME:^4} {$PORT_UP:^2} {$IS_VALID:^5} {$PORT_ENABLE:^7}' +
            ' {$SPEED:^5} {$FEC:^4} {packets_sent:^16} {bytes_sent:^16}' +
            ' {packets_received:^16} {bytes_received:^16}' +
            ' {errors_received:^16} {errors_sent:^16}' +
            ' {FCS_errors_received:^16}\n')
        header = {
            '$PORT_NAME': 'Port',
            '$PORT_UP': 'Up',
            '$IS_VALID': 'Valid',
            '$PORT_ENABLE': 'Enabled',
            '$SPEED': 'Speed',
            '$FEC': ' FEC',
            'bytes_received': 'Rx Bytes',
            'bytes_sent': 'Tx Bytes',
            'packets_received': 'Rx Packets',
            'packets_sent': 'Tx Packets',
            'errors_received': 'Rx Errors',
            'errors_sent': 'Tx Errors',
            'FCS_errors_received': 'FCS Errors'
        }

        msg = format_string.format(**header)
        for v in stats:
            msg += format_string.format(**v)
        self.out(msg)

    def do_set_switch_address(self, line):
        ''' Set switch MAC and IP to be used for SwitchML.
            Usage: set_address <MAC address> <IPv4 address>
        '''

        try:
            if not line:
                raise CommandError(
                    'Usage: set_switch_address <MAC address> <IPv4 address>')

            # Parameters validation
            args = line.upper().strip().split()
            if len(args) != 2:
                raise CommandError(
                    'Usage: set_switch_address <MAC address> <IPv4 address>')
            if not mac_address_regex.match(args[0]):
                raise CommandError('Invalid MAC address')
            if not validate_ip(args[1]):
                raise CommandError('Invalid IP address')

            self.ctrl.set_switch_mac_and_ip(args[0], args[1])

        except CommandError as e:
            self.error(e)

    def do_show_switch_address(self, line):
        ''' Show switch MAC and IP '''
        if line:
            self.error('Unknown parameter: {}'.format(line))
        else:
            mac, ip = self.ctrl.get_switch_mac_and_ip()
            self.out('Switch MAC: {} IP: {}'.format(mac, ip))

    def do_show_forwarding_table(self, line):
        ''' Show the forwarding table. If a MAC address is provided,
        only the entry for that address (if present) will be shown
        '''

        entries = []

        try:
            if line:
                mac = line.strip()
                re_match = mac_address_regex.match(mac)

                if re_match:
                    success, dev_port = self.ctrl.forwarder.get_dev_port(mac)
                    if not success:
                        raise CommandError(dev_port)

                    success, fp_port, fp_lane = self.ctrl.ports.get_fp_port(
                        dev_port)
                    if not success:
                        raise CommandError(fp_port)
                    else:
                        entry = {
                            'mac': mac,
                            'port': '{}/{}'.format(fp_port, fp_lane)
                        }
                        entries.append(entry)
                else:
                    raise CommandError('Invalid MAC address')
            else:
                for mac, dev_port in self.ctrl.forwarder.get_entries():

                    success, fp_port, fp_lane = self.ctrl.ports.get_fp_port(
                        dev_port)
                    if not success:
                        self.error(fp_port)
                    else:
                        entry = {
                            'mac': mac,
                            'port': '{}/{}'.format(fp_port, fp_lane)
                        }
                        entries.append(entry)

            if len(entries) == 0:
                self.out('No entries')
                return

            entries.sort(key=lambda x: x['port'])

            format_string = ('  {mac:^17} {port:^4}\n')
            header = {'mac': 'MAC address', 'port': 'Port'}

            msg = format_string.format(**header)
            for v in entries:
                msg += format_string.format(**v)
            self.out(msg)

        except CommandError as e:
            self.error(e)

    def do_set_drop_probabilities(self, line):
        ''' Set ingress and egress drop probabilities for simulated drops.
            Usage: set_drop_probabilities <ingress probability> <egress probability>
        '''

        try:
            if not line:
                raise CommandError(
                    'Usage: set_drop_probabilities <ingress probability> <egress probability>'
                )

            # Parameters validation
            args = line.strip().split()
            if len(args) != 2:
                raise CommandError(
                    'Usage: set_drop_probabilities <ingress probability> <egress probability>'
                )

            success, ig_prob, eg_prob = self.ctrl.drop_simulator.set_drop_probabilities(
                float(args[0]), float(args[1]))
            if not success:
                self.error(ig_prob)
            else:
                self.out('Drop probabilities: Ingress: {:.2f}% Egress: {:.2f}%'.
                         format(ig_prob * 100, eg_prob * 100))
        except ValueError:
            self.error('Drop probabilities must be real numbers')
        except CommandError as e:
            self.error(e)

    def do_show_drop_probabilities(self, line):
        ''' Show ingress and egress drop probabilities for simulated drops '''
        if line:
            self.error('Unknown parameter: {}'.format(line))
        else:
            ig_prob, eg_prob = self.ctrl.drop_simulator.get_drop_probabilities()
            self.out(
                'Drop probabilities: Ingress: {:.2f}% Egress: {:.2f}%'.format(
                    ig_prob * 100, eg_prob * 100))

    def do_show_dropped_packets(self, line):
        ''' Show the number of packets dropped by the drop simulator per-QP.
            If a queue pair number is provided, only the value for that queue pair
            will be shown.
        '''

        values = None
        if line:
            try:
                qpn = int(line.strip())
                values = self.ctrl.drop_simulator.get_counter(qpn)
            except ValueError:
                self.error('The argument must be a valid queue pair number')
                return
        else:
            values = self.ctrl.drop_simulator.get_counter()

        if not values:
            self.out('No matching queue pair number')
            return

        # Remove entries with zero packets
        values = [v for v in values if v['packets'] != 0]
        if not values:
            self.out('All values are zero')
            return

        values.sort(key=lambda x: x['QP'])

        format_string = ('{QP:^5} {packets:^10}\n')
        header = {'QP': 'QP', 'packets': 'Packets'}

        msg = format_string.format(**header)
        for v in values:
            msg += format_string.format(**v)
        self.out(msg)

    def _show_workers(self, sender, receiver, line):
        ''' Show the number of sent and received packets/bytes per worker.
            If a worker ID is provided, only the value for that worker
            will be shown.
        '''

        sent = recvd = None
        if line:
            try:
                worker_id = int(line.strip())
                recvd = receiver.get_workers_counter(worker_id)
                sent = sender.get_workers_counter(worker_id)
            except ValueError:
                self.error('The argument must be a valid worker ID')
                return
        else:
            recvd = receiver.get_workers_counter()
            sent = sender.get_workers_counter()

        if not sent and not recvd:
            self.out('No matching workers')
            return
        elif (sent and not recvd) or (not sent and recvd):
            self.log.error('Sender and Receiver entries mismatch')
            self.error('Unexpected error. See log for details')
            return

        # Merge sent and received
        for id in sent.keys():
            if id not in recvd:
                self.log.error(
                    'Sender and Receiver entries mismatch. Missing worker {}'.
                    format(id))
                self.error('Unexpected error. See log for details')
                return

            recvd[id].update(sent[id])

        ids = sorted(recvd.keys())

        header1_format_string = ' {:^44} {:^19} {:^19}\n'
        format_string = (
            ' {ID:^10} {MAC:^17} {IP:^15} {rpkts:^10}/{rbytes:^13} {spkts:^10}/{sbytes:^13}\n'
        )
        header2 = {
            'ID': 'Worker ID',
            'MAC': 'Worker MAC',
            'IP': 'Worker IP',
            'rpkts': 'Packets',
            'rbytes': 'Bytes',
            'spkts': 'Packets',
            'sbytes': 'Bytes'
        }

        msg = header1_format_string.format(
            '', 'Received', 'Sent') + format_string.format(**header2)
        for id in ids:
            msg += format_string.format(ID=id, **recvd[id])
        self.out(msg)

    def do_show_rdma_workers(self, line):
        ''' Show the number of sent and received packets/bytes per RDMA worker.
            If a worker ID is provided, only the value for that worker
            will be shown.
        '''
        self._show_workers(self.ctrl.rdma_sender, self.ctrl.rdma_receiver, line)

    def do_show_queue_pairs_counters(self, line):
        ''' Show the number of packets and messages received per queue pair.
            If one integer argument N is provided, it will show only the first
            N queue pairs per worker. If two integer arguments S and N are
            provided, it will show only the elements with indices [S, S+N]
            per worker.
        '''

        values = None
        if line:
            start = 0
            count = None
            try:
                args = line.strip().split()
                if len(args) == 1:
                    count = int(args[0])
                elif len(args) > 1:
                    start = int(args[0])
                    count = int(args[1])
                values = self.ctrl.rdma_receiver.get_queue_pairs_counters(
                    start, count)
            except ValueError:
                self.error('The arguments must be integers')
                return
        else:
            values = self.ctrl.rdma_receiver.get_queue_pairs_counters()

        if not values:
            self.out('No queue pairs currently in use')
            return

        # Remove entries with zero packets
        values = [v for v in values if v['packets'] != 0]
        if not values:
            self.out('All values are zero')
            return

        values.sort(key=lambda x: x['qp_index'])

        format_string = (
            ' {qp_index:^16}  {worker_id:^10}  {qpn:^24}  {packets:^10}  {messages:^10}  {sequence_violations:^19}\n'
        )

        header = {
            'qp_index': 'Queue Pair Index',
            'worker_id': 'Worker ID',
            'qpn': 'Worker Queue Pair Number',
            'packets': 'Packets',
            'messages': 'Messages',
            'sequence_violations': 'Sequence violations'
        }

        msg = format_string.format(**header)
        for v in values:
            msg += format_string.format(**v)
        self.out(msg)

    def do_show_udp_workers(self, line):
        ''' Show the number of sent and received packets/bytes per UDP worker.
            If a worker ID is provided, only the value for that worker
            will be shown.
        '''
        self._show_workers(self.ctrl.udp_sender, self.ctrl.udp_receiver, line)

    def do_show_bitmap(self, line):
        ''' Show the current bitmap values per slot index. The default is
            to show only the first 8 values.
            If one integer argument N is provided, it will show the first
            N elements. If two integer arguments S and N are
            provided, it will show the elements with indices [S, S+N].
        '''

        values = None
        if line:
            start = 0
            count = None
            try:
                args = line.strip().split()
                if len(args) == 1:
                    count = int(args[0])
                elif len(args) > 1:
                    start = int(args[0])
                    count = int(args[1])
                values = self.ctrl.bitmap_checker.get_bitmap(start, count)
            except ValueError:
                self.error('The arguments must be integers')
                return
        else:
            values = self.ctrl.bitmap_checker.get_bitmap()

        if not values:
            self.out('No elements found')
            return

        # Merge entries for 2 sets
        bitmap_table = {}
        pipes = set()
        for v in values:
            pipes.add(v['pipe'])

            # Convert bitmap to hex
            v['bitmap'] = '0x{:08X}'.format(v['bitmap'])

            if v['index'] not in bitmap_table:
                bitmap_table[v['index']] = {}

            bitmap_table[v['index']]['pipe{}set{}'.format(
                v['pipe'], v['set'])] = v['bitmap']

        pipes = sorted(pipes)

        # Linearize
        values = []
        for index, data in bitmap_table.items():
            data['index'] = index
            values.append(data)

        values.sort(key=lambda x: x['index'])

        format_string = '{index:^9}'
        header1 = format_string.format(index='')
        header2 = {'index': 'Index'}

        for pipe in pipes:
            header1 += '{:^24s}'.format('Pipe {}'.format(pipe))
            format_string += ('{{pipe{0}set0:^12}}'
                              '{{pipe{0}set1:^12}}').format(pipe)
            header2['pipe{}set0'.format(pipe)] = 'Set 0'
            header2['pipe{}set1'.format(pipe)] = 'Set 1'
        header1 += '\n'
        format_string += '\n'

        msg = header1 + format_string.format(**header2)
        for v in values:
            msg += format_string.format(**v)
        self.out(msg)

    def do_show_statistics(self, line):
        ''' Show the number of SwitchML packets broadcasted, recirculated
            retransmitted, and dropped per slot index.
            The default is to show only the first 8 values.
            If one integer argument N is provided, it will show the first
            N elements. If two integer arguments S and N are
            provided, it will show the elements with indices [S, S+N].
        '''

        values = None
        if line:
            start = 0
            count = None
            try:
                args = line.strip().split()
                if len(args) == 1:
                    count = int(args[0])
                elif len(args) > 1:
                    start = int(args[0])
                    count = int(args[1])
                values = self.ctrl.next_step_selector.get_counters(start, count)
            except ValueError:
                self.error('The arguments must be integers')
                return
        else:
            values = self.ctrl.next_step_selector.get_counters()

        if not values:
            self.out('No elements found')
            return

        # We need the names in this exact order
        counters_names = ['broadcast', 'recirculate', 'retransmit', 'drop']

        # Merge entries for 2 sets
        counters_table = {}
        for v in values:
            if v['index'] not in counters_table:
                counters_table[v['index']] = {}

            for name in counters_names:
                counters_table[v['index']]['{}_set{}'.format(
                    name, v['set'])] = v[name]

        # Linearize
        values = []
        for index, data in counters_table.items():
            data['index'] = index
            values.append(data)

        values.sort(key=lambda x: x['index'])

        header1_format_string = '{index:^9}'
        format_string = '{index:^9}'
        header1 = {
            'index': '',
            'broadcast': 'Broadcasted',
            'recirculate': 'Recirculated',
            'retransmit': 'Retransmitted',
            'drop': 'Dropped'
        }
        header2 = {'index': 'Index'}

        for name in counters_names:
            header1_format_string += '{{{}:^20s}}'.format(name)
            format_string += '{{{0}_set0:^10}}{{{0}_set1:^10}}'.format(name)
            header2['{}_set0'.format(name)] = 'Set 0'
            header2['{}_set1'.format(name)] = 'Set 1'
        header1_format_string += '\n'
        format_string += '\n'

        msg = header1_format_string.format(**header1) + format_string.format(
            **header2)
        for v in values:
            msg += format_string.format(**v)
        self.out(msg)
