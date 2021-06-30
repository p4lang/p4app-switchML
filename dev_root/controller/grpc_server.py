import logging

import asyncio
import ipaddress

import switchml_pb2
import switchml_pb2_grpc

from grpc import aio
from concurrent import futures

from common import PacketSize


class GRPCServer(switchml_pb2_grpc.SessionServicer,
                 switchml_pb2_grpc.SyncServicer):

    def __init__(self, ip='[::]', port=50099):

        self.log = logging.getLogger(__name__)

        self.ip = ip
        self.port = port

        # Event to stop the server
        self._stopped = asyncio.Event()

    async def _serve(self, controller):
        ''' Server task '''

        # Setup server
        self._server = aio.server()
        switchml_pb2_grpc.add_SessionServicer_to_server(self, self._server)
        switchml_pb2_grpc.add_SyncServicer_to_server(self, self._server)
        self._server.add_insecure_port('{}:{}'.format(self.ip, self.port))

        ## Barrier
        # Incrementing operation id
        self._barrier_op_id = 0
        # Worker counters and release events
        self._barrier_ctrs = {self._barrier_op_id: 0}
        self._barrier_events = {self._barrier_op_id: asyncio.Event()}

        ## Broadcast
        # Op values, bitmap and release events
        self._bcast_values = []
        self._bcast_bitmap = []
        self._bcast_events = []

        # Controller
        self.ctrl = controller

        # Start gRPC server
        await self._server.start()

    def run(self, loop, controller):
        ''' Run the gRPC server '''

        # Submit gRPC server task
        loop.create_task(self._serve(controller))

        # Run event loop
        loop.run_until_complete(self._stopped.wait())

        # Stop gRPC server
        if self._server:
            loop.run_until_complete(self._server.stop(None))
            loop.run_until_complete(self._server.wait_for_termination())

    def stop(self):
        ''' Stop the gRPC server '''

        # Stop event loop
        self._stopped.set()

    async def Barrier(self, request, context):
        ''' Barrier method.
            All the requests for the same session ID return at the same time
            only when all the requests are received.
        '''

        # Increment counter for this operation
        self._barrier_ctrs[self._barrier_op_id] += 1

        if self._barrier_ctrs[self._barrier_op_id] < request.num_workers:

            # Barrier incomplete
            tmp_id = self._barrier_op_id

            # Wait for completion event
            await self._barrier_events[tmp_id].wait()

            # Decrement counter and delete entries for this operation
            # once all are released
            self._barrier_ctrs[tmp_id] -= 1

            if self._barrier_ctrs[tmp_id] == 0:
                del self._barrier_ctrs[tmp_id]
                del self._barrier_events[tmp_id]

        else:
            # This completes the barrier -> release
            self._barrier_events[self._barrier_op_id].set()
            self._barrier_ctrs[self._barrier_op_id] -= 1

            # Create entries for next operation
            self._barrier_op_id += 1
            self._barrier_ctrs[self._barrier_op_id] = 0
            self._barrier_events[self._barrier_op_id] = asyncio.Event()

        return switchml_pb2.BarrierResponse()

    async def Broadcast(self, request, context):
        ''' Broadcast method.
            The value received from the root (with rank = root) is sent back
            to all the participants.
            The requests received before the one from the root are kept on hold
            and released when the request from the root is received.
            The ones received afterwards return immediately.
        '''

        # Remove old operations
        for idx in range(len(self._bcast_bitmap)):
            if all(self._bcast_bitmap[idx]):
                del self._bcast_bitmap[idx]
                del self._bcast_values[idx]
                del self._bcast_events[idx]

        #Scan bitmap
        idx = -1
        for idx in range(len(self._bcast_bitmap)):
            if not self._bcast_bitmap[idx][request.rank]:
                break

        if idx == -1 or self._bcast_bitmap[idx][request.rank]:
            # If there is no operation pending with the bit 0 for this worker
            # then this is a new operation
            idx += 1
            self._bcast_bitmap.append(
                [False for _ in range(request.num_workers)])
            self._bcast_values.append(None)
            self._bcast_events.append(asyncio.Event())

        if request.rank == request.root:
            # Root: write value and release
            self._bcast_values[idx] = request.value
            self._bcast_events[idx].set()

            # Set bit for this worker
            self._bcast_bitmap[idx][request.rank] = True

        else:
            # Non-root
            if self._bcast_values[idx] is None:
                # Value not available yet
                await self._bcast_events[idx].wait()

            # Set bit for this worker (after waiting)
            self._bcast_bitmap[idx][request.rank] = True

        return switchml_pb2.BroadcastResponse(value=self._bcast_values[idx])

    def RdmaSession(self, request, context):
        ''' RDMA session setup '''

        # Convert MAC to string
        mac_hex = '{:012X}'.format(request.mac)
        mac_str = ':'.join(mac_hex[i:i + 2] for i in range(0, len(mac_hex), 2))

        # Convert IP to string
        ipv4_str = str(ipaddress.ip_address(request.ipv4))

        self.log.debug(
            '# RDMA:\n Session ID: {}\n Rank: {}\n Num workers: {}\n MAC: {}\n'
            ' IP: {}\n Rkey: {}\n Pkt size: {}B\n Msg size: {}B\n QPs: {}\n'
            ' PSNs: {}\n'.format(
                request.session_id, request.rank, request.num_workers, mac_str,
                ipv4_str, request.rkey,
                str(PacketSize(request.packet_size)).split('.')[1][4:],
                request.message_size, request.qpns, request.psns))

        if not self.ctrl:
            # This is a test, return the received parameters
            return switchml_pb2.RdmaSessionResponse(
                session_id=request.session_id,
                mac=request.mac,
                ipv4=request.ipv4,
                rkey=request.rkey,
                qpns=request.qpns,
                psns=request.psns)

        if request.rank == 0:
            # This is the first message, clear out old workers state
            self.ctrl.clear_rdma_workers(request.session_id)

        # Add new worker
        success, error_msg = self.ctrl.add_rdma_worker(
            request.session_id, request.rank, request.num_workers, mac_str,
            ipv4_str, request.rkey, request.packet_size, request.message_size,
            zip(request.qpns, request.psns))
        if not success:
            self.log.error(error_msg)
            #TODO return error message
            return switchml_pb2.RdmaSessionResponse(session_id=0,
                                                    mac=0,
                                                    ipv4=0,
                                                    rkey=0,
                                                    qpns=[],
                                                    psns=[])

        # Get switch addresses
        switch_mac, switch_ipv4 = self.ctrl.get_switch_mac_and_ip()
        switch_mac = int(switch_mac.replace(':', ''), 16)
        switch_ipv4 = int(ipaddress.ip_address(switch_ipv4))

        # Mirror this worker's rkey, since the switch doesn't care
        switch_rkey = request.rkey

        # Switch QPNs are used for two purposes:
        # 1. Indexing into the PSN registers
        # 2. Differentiating between processes running on the same server
        #
        # Additionally, there are two restrictions:
        #
        # 1. In order to make debugging easier, we should
        # avoid QPN 0 (sometimes used for management) and QPN
        # 0xffffff (sometimes used for multicast) because
        # Wireshark decodes them improperly, even when the NIC
        # treats them properly.
        #
        # 2. Due to the way the switch sends aggregated
        # packets that are part of a message, only one message
        # should be in flight at a time on a given QPN to
        # avoid reordering packets. The clients will take care
        # of this as long as we give them as many QPNs as they
        # give us.
        #
        # Thus, we construct QPNs as follows.
        # - Bit 23 is always 1. This ensures we avoid QPN 0.
        # - Bits 22 through 16 are the rank of the
        #   client. Since we only support 32 clients per
        #   aggregation in the current design, we will never
        #   use QPN 0xffffff.
        # - Bits 15 through 0 are just the index of the queue;
        #   if 4 queues are requested, these bits will
        #   represent 0, 1, 2, and 3.
        #
        # So if a client with rank 3 sends us a request with 4
        # QPNs, we will reply with QPNs 0x830000, 0x830001,
        # 0x830002, and 0x830003.

        switch_qpns = [
            0x800000 | (request.rank << 16) | i
            for i, _ in enumerate(request.qpns)
        ]

        # Initial PSNs don't matter; they're overwritten by each _FIRST or _ONLY packet.
        switch_psns = [i for i, _ in enumerate(request.qpns)]

        return switchml_pb2.RdmaSessionResponse(session_id=request.session_id,
                                                mac=switch_mac,
                                                ipv4=switch_ipv4,
                                                rkey=switch_rkey,
                                                qpns=switch_qpns,
                                                psns=switch_psns)

    def UdpSession(self, request, context):
        ''' UDP session setup '''

        # Convert MAC to string
        mac_hex = '{:012X}'.format(request.mac)
        mac_str = ':'.join(mac_hex[i:i + 2] for i in range(0, len(mac_hex), 2))

        # Convert IP to string
        ipv4_str = str(ipaddress.ip_address(request.ipv4))

        self.log.debug(
            '# UDP:\n Session ID: {}\n Rank: {}\n Num workers: {}\n MAC: {}\n'
            ' IP: {}\n Pkt size: {}\n'.format(request.session_id, request.rank,
                                              request.num_workers, mac_str,
                                              ipv4_str, request.packet_size))

        if not self.ctrl:
            # This is a test, return the received parameters
            return switchml_pb2.UdpSessionResponse(
                session_id=request.session_id,
                mac=request.mac,
                ipv4=request.ipv4)

        if request.rank == 0:
            # This is the first message, clear out old workers state
            self.ctrl.clear_udp_workers(request.session_id)

        # Add new worker
        success, error_msg = self.ctrl.add_udp_worker(request.session_id,
                                                      request.rank,
                                                      request.num_workers,
                                                      mac_str, ipv4_str)
        if not success:
            self.log.error(error_msg)
            #TODO return error message
            return switchml_pb2.UdpSessionResponse(session_id=0, mac=0, ipv4=0)

        # Get switch addresses
        switch_mac, switch_ipv4 = self.ctrl.get_switch_mac_and_ip()
        switch_mac = int(switch_mac.replace(':', ''), 16)
        switch_ipv4 = int(ipaddress.ip_address(switch_ipv4))

        return switchml_pb2.UdpSessionResponse(session_id=request.session_id,
                                               mac=switch_mac,
                                               ipv4=switch_ipv4)


if __name__ == '__main__':

    # Set up gRPC server
    grpc_server = GRPCServer()

    # Run event loop for gRPC server in a separate thread
    with futures.ThreadPoolExecutor(max_workers=1) as executor:
        loop = asyncio.get_event_loop()
        future = executor.submit(grpc_server.run, loop, None)

        try:
            # Busy wait
            while True:
                pass
        except KeyboardInterrupt:
            print('\nExiting...')
        finally:
            # Stop gRPC server and event loop
            loop.call_soon_threadsafe(grpc_server.stop)

            # Wait for thread to end
            future.result()

            loop.close()

            # Flush log
            logging.shutdown()
