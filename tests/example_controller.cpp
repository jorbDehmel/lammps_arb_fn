/*
A C++ example controller. This is trivially adaptable for
arbitrary C++ controllers: Just make sure you start the
controller with the rest of the LAMMPS MPI jobs!

This is an edge repulsion system (NOT an edge dampening system).
*/

#include <boost/json/src.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mpi.h>
#include <ostream>
#include <sstream>
#include <string>

static_assert(__cplusplus >= 201100ULL, "Invalid MPICXX version!");

/// The color all ARBFN comms will be expected to have
const static int ARBFN_MPI_COLOR = 56789;

/// Uses the atom data sent by the worker to determine the force deltas
void single_particle_fix(const boost::json::value &atom, double &dfx, double &dfy, double &dfz)
{
  dfx = dfy = dfz = 0.0;

  double x, y, z, fx, fy, fz;
  x = atom.at("x").as_double();
  y = atom.at("y").as_double();
  z = atom.at("z").as_double();
  fx = atom.at("fx").as_double();
  fy = atom.at("fy").as_double();
  fz = atom.at("fz").as_double();

  // Edge repulsion
  dfx = pow(x - 10.0, -7) + pow(x + 10.0, -7);
  dfy = pow(y - 10.0, -7) + pow(y + 10.0, -7);
  dfx = (dfx < 0.0 ? -1.0 : 1.0) * fmin(abs(dfx), fmax(0.1, 1.5 * abs(fx)));
  dfy = (dfy < 0.0 ? -1.0 : 1.0) * fmin(abs(dfy), fmax(0.1, 1.5 * abs(fy)));
}

int main()
{
  // The real and discardable communicators, respectively
  MPI_Comm comm, junk_comm;

  // Initialize MPI subsystem
  MPI_Init(NULL, NULL);

  // Comm split 1 (LAMMPS internal: junk_comm is useless)
  MPI_Comm_split(MPI_COMM_WORLD, 0, 0, &junk_comm);

  // Comm split 2 (ARBFN alignment: Produced real comm)
  MPI_Comm_split(MPI_COMM_WORLD, ARBFN_MPI_COLOR, 0, &comm);

  std::cerr << __FILE__ << ":" << __LINE__ << "> "
            << "Started controller.\n"
            << std::flush;

  // For as long as there are connections left
  uintmax_t requests = 0;
  uintmax_t num_registered = 0;
  do {
    // Await some packet
    MPI_Status status;
    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &status);

    // Create buffer, load into it, then free it
    char *const buffer = new char[status._ucount + 1];
    MPI_Recv(buffer, status._ucount, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, comm, &status);
    buffer[status._ucount] = '\0';
    boost::json::object json = boost::json::parse(buffer).as_object();
    delete[] buffer;

    // Safety check
    assert(json["type"] != "waiting" && json["type"] != "ack" && json["type"] != "response");

    // Register a new worker
    if (json["type"] == "register") {
      json.clear();
      json["type"] = "ack";
      ++num_registered;

      std::stringstream s;
      std::string raw;
      s << json;
      raw = s.str();
      MPI_Send(s.str().c_str(), raw.size(), MPI_CHAR, status.MPI_SOURCE, 0, comm);
    }

    // Erase a worker
    else if (json["type"] == "deregister") {
      assert(num_registered > 0);
      --num_registered;
    }

    // Data processing
    else if (json["type"] == "request") {
      // Periodically update user
      ++requests;
      if (requests % 1000 == 0) { std::cerr << "Request #" << requests << "\n" << std::flush; }

      // Determine fix to send back
      boost::json::array list;
      for (const auto &item : json["atoms"].as_array()) {
        // Call the single-particle fix function
        double dfx, dfy, dfz;
        single_particle_fix(item, dfx, dfy, dfz);
        boost::json::object fix;
        fix["dfx"] = dfx;
        fix["dfy"] = dfy;
        fix["dfz"] = dfz;
        list.push_back(fix);
      }

      // Properly format the response
      boost::json::object json_to_send;
      json_to_send["type"] = "response";
      json_to_send["atoms"] = list;

      // Send fix data back
      std::stringstream s;
      s << json_to_send;
      const std::string raw = s.str();
      MPI_Send(s.str().c_str(), raw.size(), MPI_CHAR, status.MPI_SOURCE, 0, comm);
    }
  } while (num_registered != 0);

  std::cerr << __FILE__ << ":" << __LINE__ << "> "
            << "Halting controller\n"
            << std::flush;

  // Final barrier
  MPI_Barrier(MPI_COMM_WORLD);

  // Free comms
  MPI_Comm_free(&comm);
  MPI_Comm_free(&junk_comm);

  // Finalize
  MPI_Finalize();

  return 0;
}
