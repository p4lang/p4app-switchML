import logging

from control import Control


class Forwarder(Control):

    def __init__(self, target, gc, bfrt_info, mgid):
        # Set up base class
        super(Forwarder, self).__init__(target, gc)

        self.log = logging.getLogger(__name__)

        self.tables = [bfrt_info.table_get('pipe.Ingress.forwarder.forward')]
        self.table = self.tables[0]

        # Annotations
        self.table.info.key_field_annotation_add('hdr.ethernet.dst_addr', 'mac')

        # Multicast group ID for flood
        self.mgid = mgid

        # Keep set of mac addresses so we can delete them all without deleting the flood rule
        self.mac_addresses = {}

        # Clear table and add defaults
        self._clear()
        self.add_default_entries()

    def _clear(self):
        ''' Remove all entries (except broadcast) '''

        self.table.entry_del(self.target, [
            self.table.make_key(
                [self.gc.KeyTuple('hdr.ethernet.dst_addr', mac_address)])
            for mac_address in self.mac_addresses
        ])
        self.mac_addresses.clear()

    def add_default_entries(self):
        ''' Add broadcast and default entries '''

        # Add broadcast entry
        self.table.entry_add(self.target, [
            self.table.make_key([
                self.gc.KeyTuple('hdr.ethernet.dst_addr', 'ff:ff:ff:ff:ff:ff')
            ])
        ], [
            self.table.make_data([self.gc.DataTuple('flood_mgid', self.mgid)],
                                 'Ingress.forwarder.flood')
        ])

        # Add default entry
        self.table.default_entry_set(
            self.target,
            self.table.make_data([self.gc.DataTuple('flood_mgid', self.mgid)],
                                 'Ingress.forwarder.flood'))

    def add_entry(self, dev_port, mac_address):
        ''' Add one entry.

            Keyword arguments:
                dev_port -- dev port number
                mac_address -- MAC address reachable through the port
        '''

        self.table.entry_add(self.target, [
            self.table.make_key(
                [self.gc.KeyTuple('hdr.ethernet.dst_addr', mac_address)])
        ], [
            self.table.make_data([self.gc.DataTuple('egress_port', dev_port)],
                                 'Ingress.forwarder.set_egress_port')
        ])
        self.mac_addresses[mac_address] = dev_port

    def add_entries(self, entry_list):
        ''' Add entries.

            Keyword arguments:
                entry_list -- a list of tuples: (dev_port, mac_address)
        '''

        for (dev_port, mac_address) in entry_list:
            self.add_entry(dev_port, mac_address)

    def remove_entry(self, mac_address):
        ''' Remove one entry '''
        self.table.entry_del(self.target, [
            self.table.make_key(
                [self.gc.KeyTuple('hdr.ethernet.dst_addr', mac_address)])
        ])
        del self.mac_addresses[mac_address]

    def get_dev_port(self, mac):
        ''' Get dev port for MAC address.

            Returns:
                (success flag, dev port or error message)
        '''

        mac = mac.upper()
        if mac not in self.mac_addresses:
            return (False, 'MAC address not found')
        return (True, self.mac_addresses[mac])

    def get_macs_on_port(self, dev_port):
        ''' Get MAC addresses associated to a dev port '''

        results = []
        for mac_address, port in self.mac_addresses.items():
            if port == dev_port:
                results.append(mac_address)

        return results

    def get_entries(self):
        ''' Get all forwarding entries.

            Returns:
                list of (MAC address, dev port)
        '''

        return self.mac_addresses.items()
