/*
A C++ example controller. This is trivially adaptable for
arbitrary C++ controllers: Just make sure you start the
controller with the rest of the LAMMPS MPI jobs!
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

/**
 * @brief The color all ARBFN comms will be expected to have
 */
const static int ARBFN_MPI_COLOR = 56789;

void send_json(MPI_Comm &_comm, const int &_target, const boost::json::value &_what)
{
  std::stringstream s;
  std::string raw;

  s << _what;
  raw = s.str();
  MPI_Send(raw.c_str(), raw.size(), MPI_CHAR, _target, 0, _comm);
}

MPI_Status recv_json(MPI_Comm &_comm, const int &_source, boost::json::value &_into)
{
  MPI_Status out;
  char *buffer;
  int reg_flag, disc_flag;

  MPI_Probe(_source, MPI_ANY_TAG, _comm, &out);
  assert(out._ucount > 0);
  buffer = new char[out._ucount + 1];

  MPI_Recv(buffer, out._ucount, MPI_CHAR, out.MPI_SOURCE, out.MPI_TAG, _comm, &out);
  buffer[out._ucount] = '\0';
  _into = boost::json::parse(buffer);

  delete[] buffer;
  return out;
}

int main()
{
  boost::json::object json;
  boost::json::value json_val;
  std::string message;
  MPI_Comm comm, junk_comm;
  int world_rank;
  uintmax_t num_registered = 0;

  MPI_Init(NULL, NULL);

  // Comm split 1 (LAMMPS internal)
  MPI_Comm_split(MPI_COMM_WORLD, 0, 0, &junk_comm);

  // Comm split 2 (ARBFN alignment)
  MPI_Comm_split(MPI_COMM_WORLD, ARBFN_MPI_COLOR, 0, &comm);

  MPI_Comm_rank(comm, &world_rank);
  std::cerr << __FILE__ << ":" << __LINE__ << "> "
            << "Starting controller as " << world_rank << "\n"
            << std::flush;

  // For as long as there are connections left
  uintmax_t count = 0;
  do {
    // Await some packet
    const auto status = recv_json(comm, MPI_ANY_SOURCE, json_val);
    json = json_val.as_object();

    assert(json["type"] != "waiting" && json["type"] != "ack" && json["type"] != "response");

    // Book keeping
    if (json["type"] == "register") {
      // Register a new worker

      json.clear();
      json["type"] = "ack";

      ++num_registered;

      json_val = json;
      send_json(comm, status.MPI_SOURCE, json_val);
    }

    else if (json["type"] == "deregister") {
      // Erase a worker
      assert(num_registered > 0);
      --num_registered;
    }

    // Data processing
    else if (json["type"] == "request") {
      ++count;
      if (count % 1000 == 0) { std::cerr << "Request #" << count << "\n" << std::flush; }

      // Determine fix to send back
      boost::json::array list;
      for (const auto &item : json["atoms"].as_array()) {
        double dfx, dfy, dfz;
        single_particle_fix(item, dfx, dfy, dfz);

        boost::json::object fix;
        fix["dfx"] = dfx;
        fix["dfy"] = dfy;
        fix["dfz"] = dfz;
        list.push_back(fix);
      }

      boost::json::object json_to_send;
      json_to_send["type"] = "response";
      json_to_send["atoms"] = list;
      assert(json["atoms"].as_array().size() == json_to_send["atoms"].as_array().size());

      // Send fix data back
      send_json(comm, status.MPI_SOURCE, json_to_send);
    }
  } while (num_registered != 0);

  // Free comms
  MPI_Comm_free(&comm);
  MPI_Comm_free(&junk_comm);

  std::cerr << __FILE__ << ":" << __LINE__ << "> "
            << "Halting controller\n"
            << std::flush;
  MPI_Finalize();

  return 0;
}
