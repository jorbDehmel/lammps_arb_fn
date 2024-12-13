
# Arbitrary Single-Atom Fixes via MPI in LAMMPS
J Dehmel, J Schiffbauer

## Goals

We want to send arbitrary fix data to LAMMPS atoms as a function
of the state of the system. To do so, we will want to interface
with arbitrary external processes. We will assume one or more
"workers", each of which is running some part of the LAMMPS
simulation. For single-threaded LAMMPS, there will be only one
worker. Each worker will communicate with one central
"controller", which will calculate and send fix data.

## Design

The controller program should not be assumed to have a lifespan
less than a single update (ruling out subprocesses), nor should
it be assumed to be on the same cluster node as the LAMMPS
instance (ruling out shared memory). To avoid reinventing the
wheel and to reflect precedent, we will use MPI rather than
sockets or ports, with the controller acting as a server and
each LAMMPS instance a worker.

Firstly, we must decide on the architecture of our controller.
There are two ways this could go.

1) Many instances of LAMMPS communicate with a single instance
    of our handler program
    - All LAMMPS regions send their data to the controller,
        which would either return regional updates immediately
        or wait until all regions were received before sending
        out any data
    - This would be necessary if the controller needed data
        about every atom before yielding fixes for any of them
        (IE global ML models)
    - This would be less efficient in cases where regions could
        operate independently
2) Many instances of LAMMPS communicate with several instances
    of our handler program
    - All LAMMPS regions send their data to a controller
    - This would be more efficient if regional updates were
        possible
    - This would make certain types of computations impossible

Regional updates could also be implemented quickly in the first
case using multithreading. Since the second case makes some
calculations impossible, we will view it as a special case and
relegate it to future work.

There are two other major architectural items at play: The
controller and the receiver fix objects. It would be
advantageous to begin by describing only a protocol for
receivers so that we do not make any unnecessary assumptions
about the controller. This allows us to use arbitrary programs
(EG compiled or interpreted languages) for the controller, so
long as they compatible with MPI. We must maintain a heartbeat
with the controller so we know if the system has failed.

Every worker will operate until it is time to receive fix data
from the controller, then send a message containing all the data
the controller needs. The worker will then expect a return
message within some period of time: This return message will
either contain the fix data or will be a "heartbeat" indicating
the controller is still working. If the worker does not receive
any response, it will cause an error. This will avoid any
worker-end deadlocks.

For ease of use, we will use simple JSON objects for our
messages.

When a worker is instantiated, it will send:
```json
{
    "type": "register"
}
```

The controller will then respond:
```json
{
    "type": "acknowledge"
}
```

When a worker is destructed, it will send:
```json
{
    "type": "deregister"
}
```

When a worker is ready for an update, it will send:
```json
{
    // The number of milliseconds it will wait before an error
    "expectResponse": 100.0,

    // A list of zero or more atoms which are ready for update
    "atoms": [
        {
            // A single atom's data goes here
        }
    ]
}
```

The controller will then send one of two responses. If it needs
more time, it will send:
```json
{
    // The packet type
    "type": "waiting",

    // The number of milliseconds the worker should wait until
    // the next packet
    "expectResponse": 1000.0,
}
```

"Waiting" packets will not get responses. Once the fix data is
ready, the controller will send:
```json
{
    // The packet type
    "type": "response",

    // A list of zero or more updates. The i-th update will act
    // on the i-th sent atom
    "atoms": [
        {
            // A single fix's data
        }
    ]
}
```

The worker will then add the fixes accordingly and resume the
simulation.

## Disclaimer

FOSS under the MIT license. Supported by NSF grant
INSERT GRANT NUMBER(S) HERE.
