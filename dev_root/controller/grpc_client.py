import grpc
import ipaddress

import switchml_pb2
import switchml_pb2_grpc

if __name__ == '__main__':

    with grpc.insecure_channel('localhost:50099') as channel:
        stub = switchml_pb2_grpc.SessionStub(channel)

        session_size = 2

        # RDMA test
        for rank in range(session_size):
            response = stub.RDMASession(
                switchml_pb2.RDMASessionRequest(
                    session_id=12345,
                    rank=rank,
                    session_size=session_size,
                    mac=int(0x112233445566),
                    ipv4=int(ipaddress.ip_address("10.0.0.1")),
                    rkey=54321,
                    packet_size=3,
                    message_size=1024,
                    qpns=[1, 2, 3, 4, 5],
                    psns=[6, 7, 8, 9, 0]))

            # Convert MAC to string
            mac_hex = '{:012x}'.format(response.mac)
            mac_str = ':'.join(
                mac_hex[i:i + 2] for i in range(0, len(mac_hex), 2))

            # Convert IP to string
            ipv4_str = ipaddress.ip_address(response.ipv4).__str__()

            msg = ('# RDMA:\n Session ID: {}\n MAC: {}\n IP: {}\n Rkey: {}\n'
                   ' QPs: {}\n PSNs: {}\n').format(response.session_id, mac_str,
                                                   ipv4_str, response.rkey,
                                                   response.qpns, response.psns)

            print(msg)

        # UDP test
        for rank in range(session_size):
            response = stub.UDPSession(
                switchml_pb2.UDPSessionRequest(
                    session_id=12345,
                    rank=rank,
                    session_size=session_size,
                    mac=int(0x112233445566),
                    ipv4=int(ipaddress.ip_address("10.0.0.1")),
                    packet_size=3))

            # Convert MAC to string
            mac_hex = '{:012x}'.format(response.mac)
            mac_str = ':'.join(
                mac_hex[i:i + 2] for i in range(0, len(mac_hex), 2))

            # Convert IP to string
            ipv4_str = ipaddress.ip_address(response.ipv4).__str__()

            msg = ('# UDP:\n Session ID: {}\n MAC: {}\n IP: {}\n').format(
                response.session_id, mac_str, ipv4_str)

            print(msg)
