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

from bfrt_grpc.client import BfruntimeRpcException


class Ports(object):

    def __init__(self, target, gc, bfrt_info):

        self.log = logging.getLogger(__name__)
        self.target = target
        self.gc = gc

        # Get port table
        self.port_table = bfrt_info.table_get('$PORT')

        # Statistics table
        self.port_stats_table = bfrt_info.table_get('$PORT_STAT')

        # Front-panel port to dev port lookup table
        self.port_hdl_info_table = bfrt_info.table_get('$PORT_HDL_INFO')

        # dev port to FP port reverse lookup table (lazy initialization)
        self.dev_port_to_fp_port = None

        # List of active ports
        self.active_ports = []

        # List of ports in loopback mode
        self.loopback_ports = []

        # PktGen table to configure pktgen ports in loopback mode
        self.pktgen_port_cfg_table = bfrt_info.table_get('tf1.pktgen.port_cfg')

    def get_dev_port(self, fp_port, lane):
        ''' Convert front-panel port to dev port.

            Keyword arguments:
                fp_port -- front panel port number
                lane -- lane number

            Returns:
                (success flag, dev port or error message)
        '''
        resp = self.port_hdl_info_table.entry_get(self.target, [
            self.port_hdl_info_table.make_key([
                self.gc.KeyTuple('$CONN_ID', fp_port),
                self.gc.KeyTuple('$CHNL_ID', lane)
            ])
        ], {'from_hw': False})

        try:
            dev_port = next(resp)[0].to_dict()['$DEV_PORT']
        except BfruntimeRpcException:
            return (False, 'Port {}/{} not found!'.format(fp_port, lane))
        else:
            return (True, dev_port)

    def get_fp_port(self, dev_port):
        ''' Get front panel port from dev port.

            Returns:
                (success flag, port or error message, lane or None)
        '''

        # If we haven't filled the reverse mapping dict yet, do so
        if self.dev_port_to_fp_port is None:
            self.dev_port_to_fp_port = {}

            # Get all ports
            resp = self.port_hdl_info_table.entry_get(self.target, [],
                                                      {'from_hw': False})

            # Fill in dictionary
            for v, k in resp:
                v = v.to_dict()
                k = k.to_dict()
                self.dev_port_to_fp_port[v['$DEV_PORT']] = (
                    k['$CONN_ID']['value'], k['$CHNL_ID']['value'])

        # Look up front panel port/lane from dev port
        if dev_port in self.dev_port_to_fp_port:
            return (True,) + self.dev_port_to_fp_port[dev_port]
        else:
            return (False, 'Invalid dev port {}'.format(dev_port), None)

    def add_port(self, front_panel_port, lane, speed, fec, an):
        ''' Add one port.

            Keyword arguments:
                front_panel_port -- front panel port number
                lane -- lane within the front panel port
                speed -- port bandwidth in Gbps, one of {10, 25, 40, 50, 100}
                fec -- forward error correction, one of {'none', 'fc', 'rs'}
                autoneg -- autonegotiation, one of {'default', 'enable', 'disable'}

            Returns:
                (success flag, None or error message)
        '''

        speed_conversion_table = {
            10: 'BF_SPEED_10G',
            25: 'BF_SPEED_25G',
            40: 'BF_SPEED_40G',
            50: 'BF_SPEED_50G',
            100: 'BF_SPEED_100G'
        }

        fec_conversion_table = {
            'none': 'BF_FEC_TYP_NONE',
            'fc': 'BF_FEC_TYP_FC',
            'rs': 'BF_FEC_TYP_RS'
        }

        an_conversion_table = {
            'default': 'PM_AN_DEFAULT',
            'enable': 'PM_AN_FORCE_ENABLE',
            'disable': 'PM_AN_FORCE_DISABLE'
        }

        success, dev_port = self.get_dev_port(front_panel_port, lane)
        if not success:
            return (False, dev_port)

        if dev_port in self.active_ports:
            msg = 'Port {}/{} already in active ports list'.format(
                front_panel_port, lane)
            self.log.warning(msg)
            return (False, msg)

        self.port_table.entry_add(self.target, [
            self.port_table.make_key([self.gc.KeyTuple('$DEV_PORT', dev_port)])
        ], [
            self.port_table.make_data([
                self.gc.DataTuple('$SPEED',
                                  str_val=speed_conversion_table[speed]),
                self.gc.DataTuple('$FEC', str_val=fec_conversion_table[fec]),
                self.gc.DataTuple('$AUTO_NEGOTIATION',
                                  str_val=an_conversion_table[an]),
                self.gc.DataTuple('$PORT_ENABLE', bool_val=True)
            ])
        ])
        self.log.info('Added port: {}/{} {}G {} {}'.format(
            front_panel_port, lane, speed, fec, an))

        self.active_ports.append(dev_port)

        return (True, None)

    def add_ports(self, port_list):
        ''' Add ports.

            Keyword arguments:
                port_list -- a list of tuples: (front panel port, lane, speed, FEC string, autoneg) where:
                 front_panel_port is the front panel port number
                 lane is the lane within the front panel port
                 speed is the port bandwidth in Gbps, one of {10, 25, 40, 50, 100}
                 fec (forward error correction) is one of {'none', 'fc', 'rs'}
                 autoneg (autonegotiation) is one of {'default', 'enable', 'disable'}

            Returns:
                (success flag, None or error message)
        '''

        for (front_panel_port, lane, speed, fec, an) in port_list:
            success, error_msg = self.add_port(front_panel_port, lane, speed,
                                               fec, an)
            if not success:
                return (False, error_msg)

        return (True, None)

    def remove_port(self, front_panel_port, lane):
        ''' Remove one port.

            Keyword arguments:
                front_panel_port -- front panel port number
                lane -- lane within the front panel port

            Returns:
                (success flag, None or error message)
        '''

        success, dev_port = self.get_dev_port(front_panel_port, lane)
        if not success:
            return (False, dev_port)

        # Remove on switch
        self.port_table.entry_del(self.target, [
            self.port_table.make_key([self.gc.KeyTuple('$DEV_PORT', dev_port)])
        ])

        self.log.info('Removed port: {}/{}'.format(front_panel_port, lane))

        # Remove from our local active port list
        self.active_ports.remove(dev_port)

        return (True, None)

    def get_stats(self, front_panel_port=None, lane=None):
        ''' Get active ports statistics.
            If a port/lane is provided, it will return only stats of that port.

            Keyword arguments:
                front_panel_port -- front panel port number
                lane -- lane within the front panel port
            Returns:
                (success flag, stats or error message)
        '''

        if front_panel_port:
            if not lane:
                lane = 0

            success, dev_port = self.get_dev_port(front_panel_port, lane)
            if not success:
                return (False, dev_port)

            dev_ports = [dev_port]

            if dev_port not in self.active_ports:
                return (False,
                        'Port {}/{} not active'.format(front_panel_port, lane))
        else:
            if self.active_ports:
                dev_ports = self.active_ports
            else:
                return (False, 'No active ports')

        # Get stats
        stats_result = self.port_stats_table.entry_get(self.target, [
            self.port_stats_table.make_key([self.gc.KeyTuple('$DEV_PORT', i)])
            for i in dev_ports
        ], {'from_hw': True})

        # Construct stats dict indexed by dev_port
        stats = {}
        for v, k in stats_result:
            v = v.to_dict()
            k = k.to_dict()
            dev_port = k['$DEV_PORT']['value']
            stats[dev_port] = v

        # Get port info
        ports_info = self.port_table.entry_get(self.target, [
            self.port_table.make_key([self.gc.KeyTuple('$DEV_PORT', i)])
            for i in dev_ports
        ], {'from_hw': False})

        # Combine ports info and statistics
        values = []
        for v, k in ports_info:
            v = v.to_dict()
            k = k.to_dict()

            # Insert dev_port into result dict
            dev_port = k['$DEV_PORT']['value']
            v['$DEV_PORT'] = dev_port

            # Remove prefixes from FEC and SPEED
            v['$FEC'] = v['$FEC'][len('BF_FEC_TYP_'):]
            v['$SPEED'] = v['$SPEED'][len('BF_SPEED_'):]

            # Add port stats
            v['bytes_received'] = stats[dev_port]['$OctetsReceivedinGoodFrames']
            v['packets_received'] = stats[dev_port]['$FramesReceivedOK']
            v['errors_received'] = stats[dev_port]['$FrameswithanyError']
            v['FCS_errors_received'] = stats[dev_port][
                '$FramesReceivedwithFCSError']
            v['bytes_sent'] = stats[dev_port]['$OctetsTransmittedwithouterror']
            v['packets_sent'] = stats[dev_port]['$FramesTransmittedOK']
            v['errors_sent'] = stats[dev_port]['$FramesTransmittedwithError']

            # Add to combined list
            values.append(v)

        # Sort by front panel port/lane
        values.sort(key=lambda x: (x['$CONN_ID'], x['$CHNL_ID']))

        return (True, values)

    def reset_stats(self):
        ''' Reset statistics of all ports '''

        self.port_stats_table.entry_mod(self.target, [
            self.port_stats_table.make_key([self.gc.KeyTuple('$DEV_PORT', i)])
            for i in self.active_ports
        ], [
            self.port_stats_table.make_data([
                self.gc.DataTuple('$FramesReceivedOK', 0),
                self.gc.DataTuple('$FramesReceivedAll', 0),
                self.gc.DataTuple('$OctetsReceivedinGoodFrames', 0),
                self.gc.DataTuple('$FrameswithanyError', 0),
                self.gc.DataTuple('$FramesReceivedwithFCSError', 0),
                self.gc.DataTuple('$FramesTransmittedOK', 0),
                self.gc.DataTuple('$FramesTransmittedAll', 0),
                self.gc.DataTuple('$OctetsTransmittedwithouterror', 0),
                self.gc.DataTuple('$FramesTransmittedwithError', 0)
            ])
        ] * len(self.active_ports))

    def set_loopback_mode(self, ports):
        ''' Sets loopback mode in front panel ports.

            Keyword arguments:
                ports -- list of dev port numbers
        '''

        self.port_table.entry_add(self.target, [
            self.port_table.make_key([self.gc.KeyTuple('$DEV_PORT', dev_port)])
            for dev_port in ports
        ], [
            self.port_table.make_data([
                self.gc.DataTuple('$SPEED', str_val='BF_SPEED_100G'),
                self.gc.DataTuple('$FEC', str_val='BF_FEC_TYP_NONE'),
                self.gc.DataTuple('$LOOPBACK_MODE', str_val='BF_LPBK_MAC_NEAR'),
                self.gc.DataTuple('$PORT_ENABLE', bool_val=True)
            ])
        ] * len(ports))

        self.loopback_ports.extend(ports)

        self.log.info('{} front panel ports set in loopback mode'.format(
            len(ports)))

    def remove_loopback_ports(self):
        ''' Remove front panel ports previously set in loopback mode '''

        self.port_table.entry_del(self.target, [
            self.port_table.make_key([self.gc.KeyTuple('$DEV_PORT', dev_port)])
            for dev_port in self.loopback_ports
        ])

        self.log.info('Removed {} front panel ports in loopback mode'.format(
            len(self.loopback_ports)))

        self.loopback_ports = []

    def set_loopback_mode_pktgen(self, ports=[192, 448]):
        ''' Sets pktgen ports in loopback mode.

            Keyword arguments:
                ports -- list of pktgen dev port numbers (default [192,448])

            Returns True on success, False otherwise.
        '''

        try:
            self.pktgen_port_cfg_table.entry_add(self.target, [
                self.pktgen_port_cfg_table.make_key(
                    [self.gc.KeyTuple('dev_port', port)]) for port in ports
            ], [
                self.pktgen_port_cfg_table.make_data(
                    [self.gc.DataTuple('recirculation_enable', bool_val=True)])
            ] * len(ports))
        except Exception as e:
            self.log.exception(e)
            return False
        else:
            self.log.info('PktGen ports {} set in loopback mode'.format(ports))

    def get_loopback_mode_pktgen(self, ports=[192, 448]):
        ''' Gets loopback mode status of pktgen ports.

            Keyword arguments:
                ports -- list of pktgen dev port numbers (default [192,448])

            Returns True if all ports are in loopback mode, False otherwise.
        '''

        # Check ports state
        resp = self.pktgen_port_cfg_table.entry_get(self.target, [
            self.pktgen_port_cfg_table.make_key(
                [self.gc.KeyTuple('dev_port', port)]) for port in ports
        ], {'from_hw': False})

        loopback_mode = True
        for v, k in resp:
            v = v.to_dict()
            k = k.to_dict()

            if not v['recirculation_enable']:
                loopback_mode = False
                break
        return loopback_mode
