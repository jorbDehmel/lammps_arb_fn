#!/usr/bin/python3

'''
An example controller written in Python. This is a force
dampener.
'''

import json
from typing import Any, Tuple
from mpi4py import MPI as mpi


ARBFN_MPI_COLOR: int = 56789


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


def atom_fix(atom: Any) -> Tuple[float, float, float]:
    '''
    Determine the (dfx, dfy, dfz) values for the given atom.
    :param atom: The JSON dictionary representing a single atom
    :returns: A 3-tuple of force deltas
    '''

    dfx: float = -0.99 * atom['fx']
    dfy: float = -0.99 * atom['fy']
    dfz: float = -0.99 * atom['fz']

    return dfx, dfy, dfz


def main() -> None:
    '''
    Main function
    '''

    num_registered: int = 0
    requests: int = 0

    # Synchronization
    mpi.COMM_WORLD.Split(0)
    comm: mpi.comm = mpi.COMM_WORLD.Split(ARBFN_MPI_COLOR)

    while True:
        # Await some packet
        status: mpi.Status = mpi.Status()
        comm.Probe(status=status)

        # Allocate appropriately-sized buffer
        buffer = bytearray(status.Get_count())

        # Receive actual data
        comm.Recv(buffer, status.Get_source(), status.Get_tag(), status=status)
        msg: str = buffer[:status.Get_count()].decode('utf-8')

        # Parse to json
        j = json.loads(msg)

        # Safety check
        assert j['type'] not in ['waiting', 'ack', 'response']

        # Book keeping
        if j['type'] == 'register':
            # Register a new worker
            to_send = {'type': 'ack'}
            num_registered += 1
            send_json(comm, to_send, status)
        elif j['type'] == 'deregister':
            # Erase a worker
            num_registered -= 1
            if num_registered == 0:
                break

        # Data processing
        elif j['type'] == 'request':
            if requests % 1000 == 0:
                print(f'On request {requests}')
            requests += 1

            # Determine fix to send back
            to_send = {
                'type': 'response',
                'atoms': []
            }
            for item in j['atoms']:
                dfx, dfy, dfz = atom_fix(item)
                to_send['atoms'].append(
                    {
                        'dfx': dfx,
                        'dfy': dfy,
                        'dfz': dfz
                    }
                )

            # Send fix data back
            send_json(comm, to_send, status)

    mpi.COMM_WORLD.Barrier()


main()
