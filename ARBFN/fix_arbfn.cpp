#include "fix_arbfn.h"
#include "interchange.h"
#include "utils.h"
#include <mpi.h>
#include <string>
#include <vector>

LAMMPS_NS::FixArbFn::FixArbFn(class LAMMPS *_lmp, int _c, char **_v) : Fix(_lmp, _c, _v)
{
  // Split comm
  MPI_Comm_split(MPI_COMM_WORLD, ARBFN_MPI_COLOR, 0, &comm);

  // Handle keywords here
  max_ms = 0.0;
  every = 1;

  for (int i = 3; i < _c; ++i) {
    const char *const arg = _v[i];

    if (strcmp(arg, "maxdelay") == 0) {
      if (i + 1 >= _c) {
        error->all(FLERR, "Malformed `fix arbfn': Missing argument for `maxdelay'.");
      }
      max_ms = utils::numeric(FLERR, _v[i + 1], false, _lmp);
      ++i;
    } else if (strcmp(arg, "every") == 0) {
      if (i + 1 >= _c) {
        error->all(FLERR, "Malformed `fix arbfn': Missing argument for `every'.");
      }
      every = utils::numeric(FLERR, _v[i + 1], false, _lmp);
      ++i;
    }

    else {
      error->all(FLERR, "Malformed `fix arbfn': Unknown keyword `" + std::string(arg) + "'.");
    }
  }
}

LAMMPS_NS::FixArbFn::~FixArbFn()
{
  send_deregistration(controller_rank, comm);
  MPI_Comm_free(&comm);
}

void LAMMPS_NS::FixArbFn::init()
{
  bool res = send_registration(controller_rank, comm);
  if (!res) {
    error->all(FLERR, "`fix arbfn' failed to register with controller: Ensure it is running.");
  }

  counter = 0;
}

void LAMMPS_NS::FixArbFn::post_force(int)
{
  // Only actually post force every once in a while
  ++counter;
  if (counter < every) {
    return;
  } else {
    // Reset counter and do interchange
    counter = 0;
  }

  // Meta
  double *const *const x = atom->x;
  double *const *const v = atom->v;
  double *const *const f = atom->f;
  int *const mask = atom->mask;

  // Variables
  bool success;
  std::vector<AtomData> to_send;

  // Move from LAMMPS atom format to AtomData struct
  size_t n = 0;
  for (size_t i = 0; i < atom->nlocal; ++i) {
    if (mask[i] & groupbit) {
      AtomData to_add;
      to_add.x = x[i][0];
      to_add.y = x[i][1];
      to_add.z = x[i][2];
      to_add.vx = v[i][0];
      to_add.vy = v[i][1];
      to_add.vz = v[i][2];
      to_add.fx = f[i][0];
      to_add.fy = f[i][1];
      to_add.fz = f[i][2];
      to_send.push_back(to_add);
      ++n;
    }
  }

  // Transmit atoms, receive fix data
  FixData *to_recv = new FixData[n];
  success = interchange(n, to_send.data(), to_recv, max_ms, controller_rank, comm);
  if (!success) { error->all(FLERR, "`fix arbfn' failed interchange."); }

  // Translate FixData struct to LAMMPS force info
  n = 0;
  for (size_t i = 0; i < atom->nlocal; ++i) {
    if (mask[i] & groupbit) {
      f[i][0] += to_recv[n].dfx;
      f[i][1] += to_recv[n].dfy;
      f[i][2] += to_recv[n].dfz;
      ++n;
    }
  }
  delete[] to_recv;
}

int LAMMPS_NS::FixArbFn::setmask()
{
  int mask = 0;
  mask |= LAMMPS_NS::FixConst::POST_FORCE;
  return mask;
}
