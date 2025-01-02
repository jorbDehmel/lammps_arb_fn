/**
 * @brief Defines library functions for use in the ARBFN library
 * @author J Dehmel, J Schiffbauer, 2024, MIT License
 */

#include "interchange.h"
#include <boost/json/src.hpp>
#include <iostream>
#include <mpi.h>
#include <random>
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
 * @brief Await an MPI packet with some given source and tag for some amount of time, throwing an error if none arrives.
 * @param _max_ms The max number of milliseconds to wait before error
 * @param _into The `boost::json` to save the packet into
 * @param _recv_from The UID of the sender
 * @param _with_tag The tag needed
 * @param _comm The MPI_Comm object to use
 * @return True on success, false on failure
 */
bool await_packet(const double &_max_ms, boost::json::object &_into, const uint &_recv_from,
                  const uint &_with_tag, MPI_Comm &_comm)
{
  static std::uniform_int_distribution<uint> time_dist(0, 500);
  static std::random_device rng;

  bool got_any_packet;
  std::chrono::high_resolution_clock::time_point send_time, now;
  std::string response;
  uint64_t elapsed_us;
  MPI_Status status;
  int flag;
  char *buffer;

  got_any_packet = false;
  send_time = std::chrono::high_resolution_clock::now();
  while (!got_any_packet) {

    MPI_Iprobe(_recv_from, _with_tag, _comm, &flag, &status);

    if (flag && status._ucount > 0) {
      buffer = new char[status._ucount + 1];
      MPI_Recv(buffer, status._ucount, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, _comm, &status);
      buffer[status._ucount] = '\0';
      response = buffer;
      delete[] buffer;

      got_any_packet = true;

      std::cerr << __FILE__ << ":" << __LINE__ << "> "
                << "Got packet '" << response << "' from " << _recv_from << '\n'
                << std::flush;

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
    std::this_thread::sleep_for(std::chrono::microseconds(time_dist(rng)));
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
 * @param _comm The MPI_Comm object to use
 * @returns true on success, false on failure
 */
bool interchange(const size_t &_n, const AtomData _from[], FixData _into[], const uint &_id,
                 const double &_max_ms, const uint &_controller_rank, MPI_Comm &_comm)
{
  bool got_fix, result;
  boost::json::object json_send, json_recv;

  // Prepare and send the packet
  json_send["type"] = "request";
  json_send["expectResponse"] = _max_ms;
  json_send["uid"] = _id;
  boost::json::array list;
  for (size_t i = 0; i < _n; ++i) { list.push_back(to_json(_from[i])); }
  json_send["atoms"] = list;

  std::string to_send = json_to_str(json_send) + "\0";
  MPI_Send(to_send.c_str(), to_send.size(), MPI_CHAR, _controller_rank, ARBFN_MPI_TAG, _comm);

  // Await response
  got_fix = false;
  while (!got_fix) {
    // Await a packet from the controller
    result = await_packet(_max_ms, json_recv, _controller_rank, ARBFN_MPI_TAG, _comm);
    if (!result) {
      std::cerr << "await_packet failed\n";
      return false;
    }

    // If "waiting" packet, continue. Else, break.
    if (json_recv.at("type") == "waiting") {
      MPI_Send(to_send.c_str(), to_send.size(), MPI_CHAR, _controller_rank, ARBFN_MPI_TAG, _comm);
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
 * @param _comm The MPI_Comm object to use
 * @return The UID associated with this worker, 0 on error.
 */
uint send_registration(uint &_controller_rank, MPI_Comm &_comm)
{
  boost::json::object json;
  std::string to_send;
  int world_size, rank;
  MPI_Status status;
  bool result;
  char *buffer;

  MPI_Comm_rank(_comm, &rank);
  MPI_Comm_size(_comm, &world_size);

  to_send = "{\"type\":\"register\"}";
  for (int i = 0; i < world_size; ++i) {
    if (i != rank) {
      MPI_Send(to_send.c_str(), to_send.size(), MPI_CHAR, i, ARBFN_MPI_CONTROLLER_DISCOVER, _comm);
    }
  }

  json.clear();

  MPI_Probe(MPI_ANY_SOURCE, ARBFN_MPI_TAG, _comm, &status);
  _controller_rank = status.MPI_SOURCE;
  buffer = new char[status._ucount + 1];

  MPI_Recv(buffer, status._ucount, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, _comm, &status);
  buffer[status._ucount] = '\0';

  json = boost::json::parse(std::string(buffer)).as_object();
  delete[] buffer;

  std::cerr << __FILE__ << ":" << __LINE__ << "> "
            << "Got packet '" << json << "' from " << _controller_rank << '\n'
            << std::flush;

  return json.at("uid").as_int64();
}

/**
 * @brief Sends a deregistration packet to the controller.
 * @param _id The UID granted to this worker at registration
 * @param _comm The MPI_Comm object to use
 */
void send_deregistration(const uint &_id, const int &_controller_rank, MPI_Comm &_comm)
{
  boost::json::object to_encode;
  std::string to_send;

  to_encode["type"] = "deregister";
  to_encode["uid"] = _id;
  to_send = json_to_str(to_encode) + "\0";
  MPI_Send(to_send.c_str(), to_send.size(), MPI_CHAR, _controller_rank, ARBFN_MPI_TAG, _comm);
}
