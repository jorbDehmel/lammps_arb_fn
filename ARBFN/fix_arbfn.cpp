#include <string>
#include <vector>

#include "fix_arbfn.h"

LAMMPS_NS::FixArbFn::FixArbFn(class LAMMPS *_lmp, int _c, char **_v) : Fix(_lmp, _c, _v)
{
  // Handle keywords here
  max_ms = 2000.0;

  for (int i = 0; i < _c; ++i) {
    const char *const arg = _v[i];

    if (strcmp(arg, "maxdelay") == 0) {
      if (i + 1 >= _c) {
        error->all(FLERR, "Malformed `fix arbfn': Missing argument for `maxdelay'.");
      }
      max_ms = utils::numeric(FLERR, _v[i + 1], false, _lmp);
      ++i;
    }

    else {
      error->all(FLERR, "Malformed `fix arbfn': Unknown keyword `" + std::string(arg) + "'.");
    }
  }
}

LAMMPS_NS::FixArbFn::~FixArbFn()
{
  send_deregistration(uid, controller_rank);
}

void LAMMPS_NS::FixArbFn::init()
{
  uid = send_registration(controller_rank);
  if (uid == 0) {
    error->all(FLERR, "`fix arbfn' failed to register with controller: Ensure it is running.");
  }
}

void LAMMPS_NS::FixArbFn::post_force(int)
{
  // Meta
  const size_t n = atom->nlocal;
  double *const *const x = atom->x;
  double *const *const v = atom->v;
  double *const *const f = atom->f;

  // Variables
  bool success;
  std::vector<AtomData> to_send(n);
  std::vector<FixData> to_recv(n);

  // Move from LAMMPS atom format to AtomData struct
  for (size_t i = 0; i < n; ++i) {
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
  }

  // Transmit atoms, receive fix data
  success = interchange(n, to_send.data(), to_recv.data(), uid, max_ms, controller_rank);

  // Translate FixData struct to LAMMPS force info
  for (size_t i = 0; i < n; ++i) {
    f[i][0] += to_recv[i].dfx;
    f[i][1] += to_recv[i].dfy;
    f[i][2] += to_recv[i].dfz;
  }
}

int LAMMPS_NS::FixArbFn::setmask()
{
  int mask = 0;
  mask |= LAMMPS_NS::FixConst::POST_FORCE;
  return mask;
}
