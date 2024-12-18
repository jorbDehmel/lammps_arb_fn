
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

## Protocol

We will be sending JSON packets over MPI for communication. This
section describes the communication protocol.

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
