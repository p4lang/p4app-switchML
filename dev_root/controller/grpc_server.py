import logging

import asyncio
import ipaddress

import switchml_pb2
import switchml_pb2_grpc

from grpc import aio
from concurrent import futures


class GRPCServer(switchml_pb2_grpc.SessionServicer,
                 switchml_pb2_grpc.SynchServicer):

    def __init__(self, ip='[::]', port=50099):

        self.log = logging.getLogger(__name__)

        self.ip = ip
        self.port = port

        # Event to stop the server
        self._stopped = asyncio.Event()

    async def _serve(self):
        ''' Server task '''

        # Setup server
        self._server = aio.server()
        switchml_pb2_grpc.add_SessionServicer_to_server(self, self._server)
        switchml_pb2_grpc.add_SynchServicer_to_server(self, self._server)
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

        # Start gRPC server
        await self._server.start()

    def run(self, loop):
        ''' Run the gRPC server '''

        # Submit gRPC server task
        loop.create_task(self._serve())

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

        if self._barrier_ctrs[self._barrier_op_id] < request.size:

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
            self._bcast_bitmap.append([False for _ in range(request.size)])
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

    def RDMASession(self, request, context):
        ''' RDMA session setup '''

        # Convert MAC to string
        mac_hex = '{:012x}'.format(request.mac)
        mac_str = ':'.join(mac_hex[i:i + 2] for i in range(0, len(mac_hex), 2))

        # Convert IP to string
        ipv4_str = ipaddress.ip_address(request.ipv4).__str__()

        msg = ('# RDMA:\n Session ID: {}\n Rank: {}\n Size: {}\n MAC: {}\n'
               ' IP: {}\n Rkey: {}\n Pkt size: {}\n Msg size: {}\n QPs: {}\n'
               ' PSNs: {}\n').format(request.session_id, request.rank,
                                     request.session_size, mac_str, ipv4_str,
                                     request.rkey, request.packet_size,
                                     request.message_size, request.qpns,
                                     request.psns)

        print(msg)

        return switchml_pb2.RDMASessionResponse(session_id=request.session_id,
                                                mac=request.mac,
                                                ipv4=request.ipv4,
                                                rkey=request.rkey,
                                                qpns=request.qpns,
                                                psns=request.psns)

    def UDPSession(self, request, context):
        ''' UDP session setup '''

        # Convert MAC to string
        mac_hex = '{:012x}'.format(request.mac)
        mac_str = ':'.join(mac_hex[i:i + 2] for i in range(0, len(mac_hex), 2))

        # Convert IP to string
        ipv4_str = ipaddress.ip_address(request.ipv4).__str__()

        msg = ('# UDP:\n Session ID: {}\n Rank: {}\n Size: {}\n MAC: {}\n'
               ' IP: {}\n Pkt size: {}\n').format(request.session_id,
                                                  request.rank,
                                                  request.session_size, mac_str,
                                                  ipv4_str, request.packet_size)

        print(msg)

        return switchml_pb2.UDPSessionResponse(session_id=request.session_id,
                                               mac=request.mac,
                                               ipv4=request.ipv4)


if __name__ == '__main__':

    # Set up gRPC server
    grpc_server = GRPCServer()

    # Run event loop for gRPC server in a separate thread
    with futures.ThreadPoolExecutor(max_workers=1) as executor:
        loop = asyncio.get_event_loop()
        future = executor.submit(grpc_server.run, loop)

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
