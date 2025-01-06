#include "ARBFN/interchange.h"

#include <cassert>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <mpi.h>
#include <random>
#include <thread>
#include <vector>

const static size_t num_updates = 1000;
const static size_t num_atoms = 128;
const static double dt = 0.01;
const static double max_ms = 50.0;

int main()
{
  std::uniform_real_distribution<double> dist(-100.0, 100.0);
  std::uniform_int_distribution<uint> time_dist(0, 10000);
  std::random_device rng;
  std::vector<AtomData> atoms;
  uint controller_rank;

  MPI_Init(NULL, NULL);

  // Randomize initial atom data
  for (size_t i = 0; i < num_atoms; ++i) {
    AtomData cur;

    cur.x = dist(rng);
    cur.vx = dist(rng);
    cur.fx = dist(rng);

    cur.y = dist(rng);
    cur.vy = dist(rng);
    cur.fy = dist(rng);

    cur.z = dist(rng);
    cur.vz = dist(rng);
    cur.fz = dist(rng);

    atoms.push_back(cur);
  }

  const uint uid = send_registration(controller_rank, comm);
  std::cout << __FILE__ << ":" << __LINE__ << "> "
            << "Got controller rank " << controller_rank << '\n';
  assert(uid != 0);

  std::cout << __FILE__ << ":" << __LINE__ << "> "
            << "Worker with uid " << uid << " launched\n";

  // Collect our atoms
  const uint n = atoms.size();

  // Parameter variables
  std::vector<AtomData> atom_info_send(n);
  std::vector<FixData> fix_info_recv(n);

  for (size_t step = 0; step < num_updates; ++step) {
    // Simulate work
    std::this_thread::sleep_for(std::chrono::microseconds(time_dist(rng)));
    for (size_t j = 0; j < n; ++j) {
      atoms[j].vx += atoms[j].fx * dt;
      atoms[j].x += atoms[j].vx * dt;
      atoms[j].vy += atoms[j].fy * dt;
      atoms[j].y += atoms[j].vy * dt;
      atoms[j].vz += atoms[j].fz * dt;
      atoms[j].z += atoms[j].vz * dt;

      atom_info_send[j] = atoms[j];
    }

    // Interchange
    std::cout << __FILE__ << ":" << __LINE__ << "> "
              << "Worker " << uid << " requests interchange " << step << "\n";
    assert(interchange(n, atom_info_send.data(), fix_info_recv.data(), uid, max_ms, controller_rank,
                       comm));
    std::cout << __FILE__ << ":" << __LINE__ << "> "
              << "Worker " << uid << " got fix data " << step << "\n";

    // Update
    for (size_t j = 0; j < n; ++j) {
      atoms[j].fx += fix_info_recv[j].dfx;
      atoms[j].fy += fix_info_recv[j].dfy;
      atoms[j].fz += fix_info_recv[j].dfz;
    }
  }

  send_deregistration(uid, controller_rank, comm);

  MPI_Finalize();

  return 0;
}
