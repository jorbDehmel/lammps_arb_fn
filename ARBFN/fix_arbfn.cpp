#include "fix_arbfn.hpp"
#include "interchange.hpp"

LAMMPS_NS::FixArbFn::FixArbFn(class LAMMPS *_lmp, int _c, char **_v) : Fix(_lmp, _c, _v)
{
  uid = send_registration(0);
  assert(uid);
}

LAMMPS_NS::FixArbFn::~FixArbFn()
{
  send_deregistration(uid, 0);
}

void LAMMPS_NS::FixArbFn::post_force(int)
{
  // Meta
  bool success;
  const size_t n = ;
  std::vector<AtomData> to_send(n);
  std::vector<FixData> to_recv(n);

  // Move from LAMMPS atom format to AtomData struct
  for (size_t i = 0; i < n; ++i) { to_send.push_back(/* ... */); }

  // Transmit atoms, receive fix data
  success = interchange(n, to_send.data(), to_recv.data(), uid, 2'000.0);

  // Translate FixData struct to LAMMPS force info
  for (size_t i = 0; i < n; ++i) { /* ... */
    to_recv[i];
  }
}
