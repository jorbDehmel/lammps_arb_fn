/**
 * @brief Defines library functions for use in the ARBFN library
 * @author J Dehmel, J Schiffbauer, 2024, MIT License
 */

#include "interchange.h"
#include <boost/json/src.hpp>
#include <iostream>
#include <mpi.h>
#include <sstream>

/**
 * @brief Turn a JSON object into a std::string
 * @param _what The JSON to stringify
 * @return The string version
 */
std::string json_to_str(boost::json::value _what)
{
  std::stringstream s;
  s << _what;
  return s.str();
}

/**
 * @brief Yields a string JSON version of the given atom
 * @param _what The atom to JSON-ify
 * @return The serialized version of the atom
 */
boost::json::object to_json(const AtomData &_what)
{
  boost::json::object j;

  j["x"] = _what.x;
  j["vx"] = _what.vx;
  j["fx"] = _what.fx;
  j["y"] = _what.y;
  j["vy"] = _what.vy;
  j["fy"] = _what.fy;
  j["z"] = _what.z;
  j["vz"] = _what.vz;
  j["fz"] = _what.fz;

  return j;
}

/**
 * @brief Parses some JSON object into raw fix data.
 * @param _to_parse The JSON object to load from
 * @return The deserialized version of the object
 */
FixData from_json(const boost::json::value &_to_parse)
{
  FixData f;

  f.dfx = _to_parse.at("dfx").as_double();
  f.dfy = _to_parse.at("dfy").as_double();
  f.dfz = _to_parse.at("dfz").as_double();

  return f;
}

/**
 * @brief Await an MPI packet for some amount of time, throwing an error if none arrives.
 * @param _max_ms The max number of milliseconds to wait before error
 * @param _into The `boost::json` to save the packet into
 * @param _rng Random number generator
 * @param _time_dist Uniform int range for use w/ `_rng`
 * @param _received_from Where to save the UID of the sender
 * @return True on success, false on failure
 */
bool await_packet(const double &_max_ms, boost::json::object &_into, std::random_device &_rng,
                  std::uniform_int_distribution<uint> &_time_dist, uint &_received_from)
{
  bool got_any_packet;
  std::chrono::high_resolution_clock::time_point send_time, now;
  MPI_Comm comm = MPI_COMM_WORLD;
  std::string response;
  uint64_t elapsed_us;
  MPI_Status status;
  int flag;
  char *buffer;

  got_any_packet = false;
  send_time = std::chrono::high_resolution_clock::now();
  while (!got_any_packet) {
    // Check for message recv resolution
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &flag, &status);
    if (flag && status._ucount > 0) {
      buffer = new char[status._ucount + 1];
      MPI_Recv(buffer, status._ucount, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, comm, &status);
      buffer[status._ucount] = '\0';
      response = buffer;
      delete[] buffer;

      got_any_packet = true;
      _received_from = status.MPI_SOURCE;
      break;
    }

    // Update time elapsed
    now = std::chrono::high_resolution_clock::now();
    elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - send_time).count();

    // If it has been too long, indicate error
    if (elapsed_us / 1000.0 > _max_ms && _max_ms > 0.0) {
      // Indicate error
      std::cerr << "Timeout!\n";
      return false;
    }

    // Else, sleep for a bit
    std::this_thread::sleep_for(std::chrono::microseconds(_time_dist(_rng)));
  }

  // Unwrap packet
  _into = boost::json::parse(response).as_object();
  return true;
}

/**
 * @brief Send the given atom data, then receive the given fix data. This is blocking, but does not allow worker-side gridlocks.
 * @param _n The number of atoms/fixes in the arrays.
 * @param _from An array of atom data to send
 * @param _into An array of fix data that was received
 * @param _id The UID given to this worker at registration
 * @param _max_ms The max number of milliseconds to await each response
 * @returns true on success, false on failure
 */
bool interchange(const size_t &_n, const AtomData _from[], FixData _into[], const uint &_id,
                 const double &_max_ms, const uint &_controller_rank)
{
  bool got_fix, result;
  boost::json::object json_send, json_recv;
  MPI_Comm comm = MPI_COMM_WORLD;
  std::random_device rng;
  std::uniform_int_distribution<uint> time_dist(0, 500);
  uint received_from;

  // Prepare and send the packet
  json_send["type"] = "request";
  json_send["expectResponse"] = _max_ms;
  json_send["uid"] = _id;
  boost::json::array list;
  for (size_t i = 0; i < _n; ++i) { list.push_back(to_json(_from[i])); }
  json_send["atoms"] = list;

  std::string to_send = json_to_str(json_send) + "\0";
  MPI_Send(to_send.c_str(), to_send.size(), MPI_CHAR, _controller_rank, 0, comm);

  // Await response
  got_fix = false;
  while (!got_fix) {
    // Await any sort of packet
    result = await_packet(_max_ms, json_recv, rng, time_dist, received_from);
    if (!result) {
      std::cerr << "await_packet failed\n";
      return false;
    } else if (received_from != _controller_rank) {
      continue;
    }

    // If "waiting" packet, continue. Else, break.
    if (json_recv.at("type") == "waiting") {
      MPI_Send(to_send.c_str(), to_send.size(), MPI_CHAR, _controller_rank, 0, comm);
    } else {
      if (json_recv["type"] != "response") {
        std::cerr << "Controller sent bad packet w/ type '" << json_recv["type"] << "'\n";
        return false;
      }
      got_fix = true;
      break;
    }
  }

  // Transcribe fix data
  if (json_recv.at("atoms").as_array().size() != _n) {
    std::cerr << "Received malformed fix data from controller.\n";
    return false;
  }
  for (size_t i = 0; i < _n; ++i) { _into[i] = from_json(json_recv.at("atoms").as_array().at(i)); }

  return true;
}

/**
 * @brief Sends a registration packet to the controller.
 * @return The UID associated with this worker, 0 on error.
 */
uint send_registration(uint &_controller_rank)
{
  MPI_Comm comm = MPI_COMM_WORLD;
  boost::json::object json;
  std::random_device rng;
  std::uniform_int_distribution<uint> time_dist(0, 500);
  std::string to_send;
  int world_size, rank;
  bool result;

  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &world_size);

  json["type"] = "register";
  for (int i = 0; i < world_size; ++i) {
    if (i != rank) {
      to_send = json_to_str(json) + "\0";
      MPI_Send(to_send.c_str(), to_send.size(), MPI_CHAR, i, 0, comm);
    }
  }

  json.clear();
  do {
    result = await_packet(1000.0, json, rng, time_dist, _controller_rank);
    if (!result) { return 0; }
  } while (!json.contains("type") || json.at("type") != "ack" || !json.contains("uid"));

  return json.at("uid").as_int64();
}

/**
 * @brief Sends a deregistration packet to the controller.
 * @param _id The UID granted to this worker at registration
 */
void send_deregistration(const uint &_id, const int &_controller_rank)
{
  MPI_Comm comm = MPI_COMM_WORLD;
  boost::json::object to_encode;
  std::string to_send;

  to_encode["type"] = "deregister";
  to_encode["uid"] = _id;
  to_send = json_to_str(to_encode) + "\0";
  MPI_Send(to_send.c_str(), to_send.size(), MPI_CHAR, _controller_rank, 0, comm);
}
