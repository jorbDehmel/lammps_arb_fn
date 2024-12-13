#include "fix_arbfn.hpp"

#include <chrono>
#include <cstddef>
#include <mpi.h>
#include <string>
#include <thread>

/**
 * @brief Yields a string JSON version of the given atom
 * @param _what The atom to JSON-ify
 * @return The serialized version of the atom
 */
boost::json::object to_json(const AtomData &_what)
{
  boost::json::object out;
  return out;
}

/**
 * @brief Parses some JSON object into raw fix data.
 * @param _to_parse The JSON object to load from
 * @return The deserialized version of the object
 */
FixData from_json(const boost::json::value &_to_parse)
{
  FixData out;
  return out;
}

/**
 * Send the given atom data, then receive the given fix data.
 * This is blocking, but does not allow worker-side gridlocks.
 * @param _from An array of atom data to send
 * @param _into An array of fix data that was received
 * @returns true on success, false on failure
 */
bool interchange(const size_t &_n, const AtomData _from[], FixData _into[], const double &_max_ms, MPI_Comm &comm)
{
  bool got_fix, got_any_packet;
  std::string response, to_send;
  std::chrono::high_resolution_clock::time_point send_time, now;
  uint64_t elapsed_us;
  boost::json::object json_send, json_recv;
  size_t size;
  MPI_Request request;

  // Prepare and send the packet
  json_send["expectResponse"] = _max_ms;
  json_send["atoms"] = boost::json::array();
  for (size_t i = 0; i < _n; ++i) { json_send["atoms"].as_array().push_back(to_json(_from[i])); }
  to_send = boost::json::serialize(json_send);

  MPI_Send(to_send.c_str(), to_send.size(), MPI_CHAR, 1,
    MPI_TAG, comm);

  // Await response
  got_fix = false;
  while (!got_fix) {
    MPI_Irecv(, , , , , , );

    // Await any sort of packet
    got_any_packet = false;
    send_time = std::chrono::high_resolution_clock::now();
    while (!got_any_packet) {
      // Check for message recv resolution
      MPI_Iprobe(, , , );

      if () {
        got_any_packet = true;
        break;
      }

      // Update time elapsed
      now = std::chrono::high_resolution_clock::now();
      elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - send_time).count();

      // If it has been too long, indicate error
      if (elapsed_us / 1000.0 > _max_ms) {
        // Indicate error
        return false;
      }
    }

    // Unwrap packet
    json_recv = boost::json::parse(response).as_object();

    // If "waiting" packet, continue. Else, break.
    if (json_recv.at("type") == "waiting") {
      // Delay and continue
      std::this_thread::sleep_for(std::chrono::microseconds(rand() % 500));
    } else {
      // json_recv["type"] == "response"
      got_fix = true;
      break;
    }
  }

  // Transcribe fix data
  for (size_t i = 0; i < _n; ++i) { _into[i] = from_json(json_recv.at("atoms").as_array().at(i)); }

  return true;
}

LAMMPS_NS::FixArbFn::FixArbFn(class LAMMPS *, int, char **)
{
  // Initialize MPI and send initialization packet
}

LAMMPS_NS::FixArbFn::~FixArbFn()
{
  // Send destructor packet
}

int LAMMPS_NS::FixArbFn::setmask() {}
void LAMMPS_NS::FixArbFn::init() {}
void LAMMPS_NS::FixArbFn::post_integrate() {}
void LAMMPS_NS::FixArbFn::setup(int) {}
void LAMMPS_NS::FixArbFn::post_force(int) {}
double LAMMPS_NS::FixArbFn::memory_usage() {}
