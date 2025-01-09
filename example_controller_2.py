#!/usr/bin/python3

'''
An example controller written in Python.
'''

import json
from typing import Tuple
from mpi4py import MPI as mpi


def recv_json(comm: mpi.Comm) -> Tuple[object, mpi.Status]:
    '''
    Receive a JSON packet from any source.
    :param comm: The MPI communicator object to listen on
    :returns: A 2-tuple of the JSON received and the MPI status
    '''

    status: mpi.Status = mpi.Status()
    comm.Probe(status=status)

    # Allocate appropriately-sized buffer
    buffer = bytearray(status.Get_count())

    # Receive actual data
    comm.Recv(buffer, status.Get_source(), status.Get_tag(), status=status)

    msg: str = buffer[:status.Get_count()].decode('utf-8')

    return (json.loads(msg), status)


def send_json(comm: mpi.Comm, json_obj: object,
              prev_status: mpi.Status) -> None:
    '''
    Send a JSON packet to the sender of the previous packet
    :param comm: The MPI communicator to use
    :param json_obj: The JSON object to encode
    :param prev_status: The status of the previous recv from the now-target
    '''

    s: str = json.dumps(json_obj)
    size_to_encode: int = len(s)
    buff: bytearray = bytearray(size_to_encode)

    # Add the actual data
    for i, c in enumerate(s):
        buff[i] = ord(c)

    assert buff.decode('utf-8') == s

    # Send the raw buffer
    comm.Send(buff, prev_status.Get_source(), 0)

    del buff


def main() -> None:
    '''
    Main function
    '''

    _ = mpi.COMM_WORLD.Split(0)
    comm: mpi.comm = mpi.COMM_WORLD.Split(56789)
    num_registered: int = 0

    while True:
        # Await some packet
        j, status = recv_json(comm)
        print(
            f'Got message w/ type {j["type"]} from worker {status.Get_source()}')

        assert j['type'] not in ['waiting', 'ack', 'response']

        if j['type'] == 'register':
            # Register a new worker
            to_send = {
                'type': 'ack'
            }

            num_registered += 1
            send_json(comm, to_send, status)

        elif j['type'] == 'deregister':
            # Erase a worker

            num_registered -= 1

            if num_registered == 0:
                break

        # Data processing
        elif j['type'] == 'request':
            # Determine fix to send back
            l = []

            for item in j['atoms']:
                l.append(
                    {'dfx': item['fx'],
                        'dfy': item['fy'],
                        'dfz': item['fz']}
                )

            to_send = {
                'type': 'response',
                'atoms': l
            }
            assert len(to_send['atoms']) == len(j['atoms'])

            # Send fix data back
            send_json(comm, to_send, status)


main()
