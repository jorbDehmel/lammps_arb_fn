/*
Another C++ example controller. This is trivially adaptable for
arbitrary C++ controllers: Just make sure you start the
controller with the rest of the LAMMPS MPI jobs!

This is a bulk controller, which must wait for all regions to
report before responding to any of them. This demonstrates the
"waiting" packet type.

This specific controller mimics gravity.
*/

#include <boost/json/array.hpp>
#include <boost/json/src.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <mpi.h>
#include <ostream>
#include <sstream>
#include <string>

static_assert(__cplusplus >= 201100ULL, "Invalid MPICXX version!");

/// The color all ARBFN comms will be expected to have
const static int ARBFN_MPI_COLOR = 56789;

int main()
{
  MPI_Comm comm, junk_comm;
  MPI_Init(NULL, NULL);
  MPI_Comm_split(MPI_COMM_WORLD, 0, 0, &junk_comm);
  MPI_Comm_split(MPI_COMM_WORLD, ARBFN_MPI_COLOR, 0, &comm);

  std::cerr << __FILE__ << ":" << __LINE__ << "> "
            << "Started controller.\n"
            << std::flush;

  // For as long as there are connections left
  uintmax_t requests = 0;
  uintmax_t num_registered = 0;

  // Bulk controlling: Maps worker rank to atom data
  // Cleared after every successful step
  std::map<int, boost::json::array> bulk_received;

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

    // Bookkeeping
    if (json["type"] == "register") {
      ++num_registered;
      const std::string raw = "{\"type\": \"ack\"}";
      MPI_Send(raw.c_str(), raw.size(), MPI_CHAR, status.MPI_SOURCE, 0, comm);
    } else if (json["type"] == "deregister") {
      --num_registered;
    }

    // Data processing
    else if (json["type"] == "request") {
      // Synchronization stuff
      bulk_received[status.MPI_SOURCE] = json.at("atoms").as_array();
      if (bulk_received.size() != num_registered) {
        // Send waiting packet and continue
        const std::string msg = "{\"type\": \"waiting\"}";
        MPI_Send(msg.c_str(), msg.size(), MPI_CHAR, status.MPI_SOURCE, 0, comm);
        continue;
      }

      if (++requests % 1000 == 0) { std::cerr << "Request #" << requests << "\n" << std::flush; }

      // Find midpoint
      double mean_x = 0.0, mean_y = 0.0;
      uintmax_t count = 0;
      for (const auto &p : bulk_received) {
        for (const auto &item : p.second) {
          ++count;
          mean_x += item.at("x").as_double();
          mean_y += item.at("y").as_double();
        }
      }
      mean_x /= count;
      mean_y /= count;

      for (const auto &p : bulk_received) {
        boost::json::array list;
        for (const auto &item : p.second) {
          const double dx = mean_x - item.at("x").as_double();
          const double dy = mean_y - item.at("y").as_double();
          const double distance = sqrt(pow(dx, 2) + pow(dy, 2));

          double dfx = dx / distance;
          if (abs(dfx) > 0.1) { dfx = (dfx > 0 ? 0.1 : -0.1); }

          double dfy = dy / distance;
          if (abs(dfy) > 0.1) { dfy = (dfy > 0 ? 0.1 : -0.1); }

          boost::json::object fix;
          fix["dfx"] = dfx;
          fix["dfy"] = dfy;
          fix["dfz"] = 0.0;

          list.push_back(fix);
        }

        boost::json::object json_to_send;
        std::stringstream s;
        json_to_send["type"] = "response";
        json_to_send["atoms"] = list;
        s << json_to_send;
        const std::string raw = s.str();
        MPI_Send(raw.c_str(), raw.size(), MPI_CHAR, p.first, 0, comm);
      }

      bulk_received.clear();
    }
  } while (num_registered != 0);

  std::cerr << __FILE__ << ":" << __LINE__ << "> "
            << "Halting controller\n"
            << std::flush;

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_free(&comm);
  MPI_Comm_free(&junk_comm);
  MPI_Finalize();
  return 0;
}
