
# Arbitrary Single-Atom Fixes via MPI in LAMMPS

![Test Badge](https://github.com/jorbDehmel/lammps_arb_fn/actions/workflows/ci-test.yml/badge.svg)

J Dehmel, J Schiffbauer, 2024 (MIT License)

## Goals

We want to send arbitrary fix data to LAMMPS atoms as a function
of the state of the system. To do so, we will want to interface
with arbitrary external processes. We will assume one or more
"workers", each of which is running some part of the LAMMPS
simulation. For single-threaded LAMMPS, there will be only one
worker. Each worker will communicate with one central
"controller", which will calculate and send fix data.

## Requirements and Testing

This software is built to work with the LAMMPS source code, and
is such written in `C++` using MPI. The following are
requirements for compilation:
- `g++` and `make` (you almost certainly already have these)
- Some MPI provider (EG `openmpi`: You probably also have this
    if you are compiling LAMMPS)
- `boost` library for `C++` (specifically `boost_json`)

The following software is required for testing, but not
necessarily for non-testing compilation.
- `python3`
- `python3-pip`
- `mpi4py`
- `git`
- (Optional) `docker`

To test the source code, run
```sh
make test
```
from this directory. More details on installing it inside a
LAMMPS binary will be given later. To enter a development
`Docker` container, run `make launch-docker` from this
directory.

## Installation

**TODO: Write this section**

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
            "x": 123.0,
            "vx": 123.0,
            "fx": 123.0,
            "y": 123.0,
            "vy": 123.0,
            "fy": 123.0,
            "z": 123.0,
            "vz": 123.0,
            "fz": 123.0
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
            "dfx": 1.23, // How much to change fx
            "dfy": 1.23, // How much to change fy
            "dfz": 1.23  // How much to change fz
        }
    ]
}
```

The worker will then add the fixes accordingly and resume the
simulation.

## Disclaimer

FOSS under the MIT license. Supported by NSF grant
INSERT GRANT NUMBER(S) HERE.
