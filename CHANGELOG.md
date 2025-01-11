
# Todo
- Currently hangs in error states: Make it shut down properly

# Changelog

## `0.1.2` (1/10/2025)
- Added support for dipole moments (orientation components and
    magnitude $\mu$) via the `dipole` fix argument
- Normalized doxygen in headers

## `0.1.1` (1/9/2025)
- Added some integration testing
- Reorganized repo to unclutter root dir
- Fixed formatting in INSTALL.py
- Removed most output from tests
- Fixed bug causing hang w/ `waiting` packets

## `0.1.0` (1/9/2025)
- Got LAMMPS integration functional
- Documentation improvements
- Added LAMMPS test script
- Added INSTALL.py for quick LAMMPS builds

## `0.0.2` (12/20/2024)
- Got LAMMPS compile functional
- Removed dependency on `boost::mpi` (now just using `boost` for
    JSON)
- Minor documentation improvements

## `0.0.1` (12/16/2024)
- Added minimal functional controller protocol implementations
    in `C++` and `python`
- Wrote out protocol in `README`

## `0.0.0` (12/12/2024)
- Initial commit
