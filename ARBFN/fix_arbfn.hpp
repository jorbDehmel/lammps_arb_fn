/* -*- c++ -*- ----------------------------------------------------------
    LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
    https://www.lammps.org/, Sandia National Laboratories
    LAMMPS development team: developers@lammps.org

    Copyright (2003) Sandia Corporation.  Under the terms of Contract
    DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
    certain rights in this software.  This software is distributed under
    the GNU General Public License.

    See the README file in the top-level LAMMPS directory.
-------------------------------------------------------------------------
    Defines the fix_arbfn class for extending LAMMPS. Based on FixQMMM
    from the QMMM package. Based on work funded by NSF grant
    <TODO: GRANT NUMBER HERE!!!!>

    J Dehmel, J Schiffbauer, 2024
------------------------------------------------------------------------- */

#ifndef FIX_ARBFN_HPP
#define FIX_ARBFN_HPP

// #include "fix.h"
#include "/home/jorb/Programs/mylammps/src/fix.h"

#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include <mpi.h>

// FIX THIS
const static int MPI_TAG = -1;

struct AtomData {};
struct FixData {};

/**
 * @brief Yields a string JSON version of the given atom
 * @param _what The atom to JSON-ify
 * @return The serialized version of the atom
 */
boost::json::object to_json(const AtomData &_what);

/**
 * @brief Parses some JSON object into raw fix data.
 * @param _to_parse The JSON object to load from
 * @return The deserialized version of the object
 */
FixData from_json(const boost::json::value &_to_parse);

/**
 * Send the given atom data, then receive the given fix data.
 * This is blocking, but does not allow worker-side gridlocks.
 * @param _from An array of atom data to send
 * @param _into An array of fix data that was received
 * @returns true on success, false on failure
 */
bool interchange(const size_t &_n, const AtomData _from[], FixData _into[], const double &_max_ms);

namespace LAMMPS_NS {
class FixArbFn : public Fix {
 public:
  FixArbFn(class LAMMPS *, int, char **);
  ~FixArbFn() override;

  int setmask() override;
  void init() override;

  // send up-to-date position information
  void post_integrate() override;

  // receive and update forces
  void setup(int) override;
  void post_force(int) override;

  double memory_usage() override;

 protected:
  MPI_Comm comm;    // intra communicator with QM subsystem
};
}    // namespace LAMMPS_NS

#endif
