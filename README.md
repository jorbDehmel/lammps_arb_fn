
# Arbitrary Single-Atom Fixes via MPI in LAMMPS

![Test Badge](https://github.com/jorbDehmel/lammps_arb_fn/actions/workflows/ci-test.yml/badge.svg)

J Dehmel, J Schiffbauer, 2024/25 (MIT License)

## Abstract

**TODO: Write this**

## Requirements and Testing

This software is built to work with the LAMMPS source code, and
is such written in `C++` using MPI. The following are
requirements for compilation:
- `g++` and `make` (you almost certainly already have these)
- `CMake`
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

To check your system for the given requirements, run
```sh
make check
```
from this directory.

To test the source code, run
```sh
make test
```
from this directory. More details on installing it inside a
LAMMPS binary will be given later. To enter a development
`Docker` container, run `make launch-docker` from this
directory.

After compiling and install LAMMPS with the extension, a simple
testing script can be found in `./lmp_test`. You can run it
(on a 4-core or more system) via `make run`.

## Installation

This package is currently only provided standalone, and thus
requires **a full re-compilation of LAMMPS from source**. This
document will assume a `CMake` build, rather than a traditional
`make` build. Please see
[the LAMMPS docs on building from source](https://docs.lammps.org/Build.html)
for more details.

Since this process is not already in the LAMMPS `CMake` system,
it provides `INSTALL.py` locally to facilitate installation.
This script first locates the LAMMPS source code repository,
then copies the `ARBFN` source code into it, then modifies
`CMake` so that it will build properly, then optionally runs the
`CMake` installation process.

To run the installation script, simply run
```sh
python3 INSTALL.py
```
from this directory and follow the instructions given.

## Using the Fix

The `ARBFN` package provides the `arbfn` fix, shown below.

```lammps
fix name_1 all arbfn
fix name_2 all arbfn maxdelay 50.0
fix name_3 all arbfn every 100
fix name_4 all arbfn maxdelay 50.0 every 100
```

The `maxdelay X` (where `X` is the max number of milliseconds to
await a response before erroring, with $0.0$ ms meaning no
limit) and `every Y` (where the fix is applied every `Y`
timesteps, with $1$ being every step and $0$ being undefined)
arguments are both optional. The default max delay is $0.0$ (no
limit) and the default periodicity is $1$ (apply every
time step).

There is also the `dipole` argument, which includes the values
`"mu"`, `"mux"`, `"muy"`, `"muz"` from LAMMPS for each atom.

```lammps
fix name_5 all arbfn dipole
```

## Running Simulations

Although LAMMPS is built on MPI, extra care is needed when
running it alongside other processes. Namely, the `-mpicolor X`
command-line argument must be used, where `X` is any number
**except** 56789 (with 56789 being reserved for internal ARBFN
communication). Note that
**`-mpicolor` must be the first argument after `lmp`**.
Additionally, `lmp` must be launched by an `mpirun` call
**wherein the controller is also launched**. Only one instance
of the controller should be used, but arbitrarily many instances
of LAMMPS are allowed.

For instance, to launch one instance of the local controller
executable `./controller` alongside 3 instances of LAMMPS with
color $123$ on `input_script.lmp`:

```sh
mpirun -n 1 \
    ./controller \
    : -n 3 \
    lmp -mpicolor 123 -in input_script.lmp
```

To use the same setup, but instead use the `python` controller
`./py_controller.py`:

```sh
mpirun -n 1 \
    python3 ./py_controller.py \
    : -n 3 \
    lmp -mpicolor 123 -in input_script.lmp
```

## Protocol

This section uses pseudocode and standard MPI calls to outline
the protocol used from both the controller and worker
perspective. The reader should assume that there is exactly one
controller and at least one LAMMPS "worker".

We begin by describing the protocol from the controller's
perspective.

1) (SERVER) Initial MPI setup
    - Call `MPI_Init` to initialize the system
    - Call `MPI_Comm_split` to split `MPI_COMM_WORLD` off into
        a "junk" communicator which can then be discarded. This
        is necessary because LAMMPS uses an internal
        `MPI_Comm_split` upon instantiation, and all processes
        in the world must make the call before the process will
        advance. If this is not done, the system will hang
        without error indefinitely.
    - Call `MPI_Comm_split` **for the 2nd time**, this time
        splitting `MPI_COMM_WORLD` off using the color $56789$
        (the color all our MPI comms are expected to have) into
        a communicator which we save. This will be the
        communicator that we use for the remainder of the
        protocol, and corresponds to the splitting off of the
        `ARBFN` fixes from the default LAMMPS communicator. The
        same synchronization issues will occur upon omission of
        this step as the previous.
2) (SERVER) Enter server loop
    - Unless exited, repeat this step (2) forever after completion
    - Make a call to `MPI_Probe` with any source and tag,
        storing the resulting information. Since we are using
        the colored communicator, this will only every receive
        messages from the worker instances.
    - Upon receiving the non-null-terminated ASCII string
        message, save it to a string and decode it into a JSON
        object. The JSON object is guaranteed to have an
        attribute with the key `"type"`.
        - If `"type"` is the string `"register"`, increment some
            counter of the number of registered workers and send
            back a JSON packet with type `"ack"`.
        - If `"type"` is the string `"deregister"`, decrement
            the aforementioned counter. If it is now zero, exit
            the server loop. This is the only case in which the
            server shuts down.
        - If `"type"` is the string `"request"`, the JSON will
            encode the data (at minimum "x", "y", "z", "vx",
            "vy", "vz", "fx", "fy", and "fz) of each atom it
            owns into a list with the key `"atoms"`. The
            controller is expected to respond with either a
            packet of type `"waiting"` (requiring no additional
            information but prompting the worker to resent the
            packet after some interval) or a packet of type
            `"response"`. A `"response"` packet will have a list
            called `"atoms"` where the $i^\texttt{th}$ entry
            corresponds to the $i^\texttt{th}$ atom in the
            request. Each atom in the response will have (at
            minimum) "dfx", "dfy", and "dfz". Each of these will
            be a double corresponding to the prescribed deltas
            in force for their respective dimension.
3) (SERVER) Shutdown
    - After all workers have send `"deregister"` packets, LAMMPS
        will begin shutting down. This entails one final MPI
        barrier, so we the controller must call `MPI_Barrier`
        on **`MPI_COMM_WORLD`**. After this, LAMMPS will halt.
    - Now, the controller must shut down and free its resources
        in the traditional `C` MPI way (EG `MPI_Comm_free` and
        `MPI_Finalize`). The server can now halt. If improperly
        synced, the system will hang without error.

This ends our description of the controller protocol. We will
now describe the worker side of the protocol, omitting any
information which can be deduced from the above.

1) (WORKER) LAMMPS internal setup
    - Since our workers are fix objects, they have no say in the
        initial MPI setup of LAMMPS. This is where the first
        `MPI_Comm_split` call synchronization occurs.
2) (WORKER) Additional setup
    - Upon fix object instantiation, we must make a second
        `MPI_Comm_split` splitting `MPI_COMM_WORLD` into a
        usable communicator with the color $56789$. This
        corresponds with our second synchronization call on the
        controller side.
3) (WORKER) Controller discovery
    - At instantiation, our fix does not know the rank of the
        controller. Thus, the worker will iterate through all
        non-self ranks in the communicator and send a
        `"registration"` packet to each one. All but one of the
        recipients will be workers and not respond, but the one
        that sends back an `"ack"` packet will be recorded as
        the controller.
4) (WORKER) Work
    - For as long as LAMMPS lives, it will call the fix to do
        work in the form of the `post_force` procedure. This
        will iterate through the owned atoms of this worker
        instance, send them in the aforementioned format to the
        controller, receive the controller's prescription, and
        add the force deltas.
5) (WORKER) Deregistration and cleanup
    - Once LAMMPS is done, the fix destructor will be called.
        This method must send the deregistration packet to the
        controller and free up any resources used (MPI or
        standard). The worker **does not** need to call
        `MPI_Barrier`, unlike the controller.

## Disclaimer

FOSS under the MIT license. Supported by NSF grant
INSERT GRANT NUMBER(S) HERE.
