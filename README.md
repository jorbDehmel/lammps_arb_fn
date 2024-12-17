
# Arbitrary Single-Atom Fixes via MPI in LAMMPS

![Test Badge](https://github.com/jorbDehmel/lammps_arb_fn/actions/workflows/ci-test.yml/badge.svg)

J Dehmel, J Schiffbauer

**Note:** Requires `libboost` for `JSON` and `MPI`.

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

When a worker is instantiated, it will *broadcast* to all other
nodes:
```json
{
    "type": "register"
}
```

The controller (and no-one else) will then respond:
```json
{
    "type": "ack",
    "uid": 1234
}
```
where 'uid' is an arbitrary unique ID number for the worker (do
not assume that this is the same as the node's MPI rank).

In the `register`/`ack` process, the worker will have also
identified which node is the controller. When a worker is
destructed, it will send:
```json
{
    "type": "deregister",
    "uid": 1234
}
```

When a worker is ready for an update, it will send:
```json
{
    "type": "request",
    "uid": 1234,

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
If the given deadline is exceeded, an error will be thrown.

The controller will then send one of two responses. If it needs
more time, it will send:
```json
{
    // The packet type
    "type": "waiting",

    // The number of milliseconds the worker should wait until
    // the next packet
    "expectResponse": 1000.0
}
```

"Waiting" packets will prompt a precisely repeated request from
the worker. Once the fix data is ready, the controller will
send:
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

## Interfacing with `boost::mpi`'s `std::string` Serialization

Although this protocol was designed to be minimally language
dependant, the use of `C++`'s `boost::mpi` library for `MPI`
communication has imposed some extra restrictions upon its
implementation. Namely, `boost` serialization of `std::string`s
(used for encoding JSON) is not simply an array of raw bytes: It
instead first sends a 4-byte unsigned integer containing the
number of bytes to follow, then followed by the bytes of the
string buffer. For instance, if it was to send
`std::string("{\"fizz\":123}")`, it would instead transmit 4
bytes representing the unsigned integer $12$ followed by the
bytes representing the ASCII string `{"fizz":123}`. This is not
of note to the average user, but non-`C++` controller
implementations will have to make note of this. An example
avoiding this pitfall can be found in
`./example_controller_2.py`.

## Disclaimer

FOSS under the MIT license. Supported by NSF grant
INSERT GRANT NUMBER(S) HERE.
