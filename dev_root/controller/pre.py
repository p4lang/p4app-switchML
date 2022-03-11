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
from bfrt_grpc.bfruntime_pb2 import TableModIncFlag


class PRE(Control):

    def __init__(self, target, gc, bfrt_info):
        # Set up base class
        super(PRE, self).__init__(target, gc)

        self.log = logging.getLogger(__name__)

        self.tables = [
            bfrt_info.table_get('$pre.mgid'),
            bfrt_info.table_get('$pre.node'),
            bfrt_info.table_get('$pre.ecmp'),
            bfrt_info.table_get('$pre.lag'),
            bfrt_info.table_get('$pre.prune'),
            bfrt_info.table_get('$pre.port')
        ]

        self.multicast_group = self.tables[0]
        self.node = self.tables[1]
        self.port = self.tables[5]

        # Clear tables and add defaults
        self._clear()

    def _clear(self):
        ''' Remove all existing multicast groups and nodes '''

        resp = self.multicast_group.entry_get(self.target,
                                              flags={'from_hw': False})
        for _, k in resp:
            self.multicast_group.entry_del(self.target, [k])

        resp = self.node.entry_get(self.target, flags={'from_hw': False})
        for _, k in resp:
            self.node.entry_del(self.target, [k])

    def add_multicast_group(self, mgid):
        ''' Add an empty multicast group.

            Keyword arguments:
                mgid -- multicast group ID
        '''
        self.multicast_group.entry_add(
            self.target,
            [self.multicast_group.make_key([self.gc.KeyTuple('$MGID', mgid)])],
            [
                self.multicast_group.make_data([
                    self.gc.DataTuple('$MULTICAST_NODE_ID', int_arr_val=[]),
                    self.gc.DataTuple('$MULTICAST_NODE_L1_XID_VALID',
                                      bool_arr_val=[]),
                    self.gc.DataTuple('$MULTICAST_NODE_L1_XID', int_arr_val=[])
                ])
            ])

    def add_multicast_node(self, mgid, rid, port):
        ''' Create a multicast node and add it to a multicast group.

            Keyword arguments:
                mgid -- multicast group ID
                rid -- node ID
                port -- device port for the node

            Returns:
                (success flag, None or error message)
        '''
        # Check if multicast group exists
        resp = self.multicast_group.entry_get(self.target,
                                              flags={'from_hw': False})
        found = False
        for _, k in resp:
            if mgid == k.to_dict()['$MGID']['value']:
                found = True
        if not found:
            error_msg = 'Multicast group {} not present'.format(mgid)
            self.log.error(error_msg)
            return (False, error_msg)

        # Check if a node with the same ID exists
        resp = self.node.entry_get(self.target, flags={'from_hw': False})
        for _, k in resp:
            if k.to_dict()['$MULTICAST_NODE_ID']['value'] == rid:
                error_msg = 'Multicast node {} already present'.format(rid)
                self.log.error(error_msg)
                return (False, error_msg)

        # Add node
        self.node.entry_add(
            self.target,
            [self.node.make_key([self.gc.KeyTuple('$MULTICAST_NODE_ID', rid)])],
            [
                self.node.make_data([
                    self.gc.DataTuple('$MULTICAST_RID', rid),
                    self.gc.DataTuple('$DEV_PORT', int_arr_val=[port])
                ])
            ])

        # Extend multicast group
        self.multicast_group.entry_mod_inc(self.target, [
            self.multicast_group.make_key([self.gc.KeyTuple('$MGID', mgid)])
        ], [
            self.multicast_group.make_data([
                self.gc.DataTuple('$MULTICAST_NODE_ID', int_arr_val=[rid]),
                self.gc.DataTuple('$MULTICAST_NODE_L1_XID_VALID',
                                  bool_arr_val=[True]),
                self.gc.DataTuple('$MULTICAST_NODE_L1_XID', int_arr_val=[rid])
            ])
        ], TableModIncFlag.MOD_INC_ADD)
        return (True, None)

    def add_multicast_nodes(self, mgid, rids_and_ports):
        ''' Create multiple multicast nodes and add them to one multicast group.

            Keyword arguments:
                mgid -- multicast group ID
                rids_and_ports -- list of tuples (node ID, device port)

            Returns:
                (success flag, None or error message)
        '''
        for rid, port in rids_and_ports:
            success, error_msg = self.add_multicast_node(mgid, rid, port)
            if not success:
                return (False, error_msg)
        return (True, None)

    def remove_multicast_node(self, rid):
        ''' Remove multicast node with given ID '''

        resp = self.multicast_group.entry_get(self.target,
                                              flags={'from_hw': False})
        # If the node is in any group
        for v, k in resp:
            if rid in v.to_dict()['$MULTICAST_NODE_ID']:
                # Remove group entry
                self.multicast_group.entry_mod_inc(
                    self.target, [k], [
                        self.multicast_group.make_data([
                            self.gc.DataTuple('$MULTICAST_NODE_ID',
                                              int_arr_val=[rid]),
                            self.gc.DataTuple('$MULTICAST_NODE_L1_XID_VALID',
                                              bool_arr_val=[False]),
                            self.gc.DataTuple('$MULTICAST_NODE_L1_XID',
                                              int_arr_val=[0])
                        ])
                    ], TableModIncFlag.MOD_INC_DELETE)

        # Remove node entry
        self.node.entry_del(
            self.target,
            [self.node.make_key([self.gc.KeyTuple('$MULTICAST_NODE_ID', rid)])])

    def remove_multicast_group(self, mgid):
        ''' Remove multicast group with given ID '''
        self.multicast_group.entry_del(
            self.target,
            [self.multicast_group.make_key([self.gc.KeyTuple('$MGID', mgid)])])
