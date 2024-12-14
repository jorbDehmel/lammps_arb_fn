#include "interchange.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/mpi.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi/environment.hpp>
#include <chrono>
#include <cstddef>
#include <mpi.h>
#include <random>
#include <thread>

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

FixData from_json(const boost::json::value &_to_parse)
{
  FixData f;

  f.dfx = _to_parse.at("dfx").as_double();
  f.dfy = _to_parse.at("dfy").as_double();
  f.dfz = _to_parse.at("dfz").as_double();

  return f;
}

/**************************************************************/

bool await_packet(const double &_max_ms, boost::json::object &_into, std::random_device &rng,
                  std::uniform_int_distribution<uint> &time_dist, const int &_controller_rank)
{
  bool got_any_packet;
  std::chrono::high_resolution_clock::time_point send_time, now;
  boost::mpi::communicator comm;
  std::string response;
  uint64_t elapsed_us;

  got_any_packet = false;
  send_time = std::chrono::high_resolution_clock::now();
  while (!got_any_packet) {
    // Check for message recv resolution
    if (comm.iprobe(_controller_rank).has_value()) {
      comm.recv(_controller_rank, boost::mpi::any_tag, response);
      got_any_packet = true;
      break;
    }

    // Update time elapsed
    now = std::chrono::high_resolution_clock::now();
    elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - send_time).count();

    // If it has been too long, indicate error
    if (elapsed_us / 1000.0 > _max_ms) {
      // Indicate error
      std::cerr << "Timeout!\n" << std::flush;
      return false;
    }

    // Else, sleep for a bit
    std::this_thread::sleep_for(std::chrono::microseconds(time_dist(rng)));
  }

  // Unwrap packet
  _into = boost::json::parse(response).as_object();
  return true;
}

bool interchange(const size_t &_n, const AtomData _from[], FixData _into[], const uint &_id,
                 const double &_max_ms, const int &_controller_rank)
{
  bool got_fix, result;
  boost::json::object json_send, json_recv;
  boost::mpi::communicator comm;
  std::random_device rng;
  std::uniform_int_distribution<uint> time_dist(0, 500);

  // Prepare and send the packet
  json_send["type"] = "request";
  json_send["expectResponse"] = _max_ms;
  json_send["uid"] = _id;
  boost::json::array list;
  for (size_t i = 0; i < _n; ++i) { list.push_back(to_json(_from[i])); }
  json_send["atoms"] = list;
  comm.send(_controller_rank, 0, boost::json::serialize(json_send));

  // Await response
  got_fix = false;
  while (!got_fix) {
    // Await any sort of packet
    result = await_packet(_max_ms, json_recv, rng, time_dist, _controller_rank);
    if (!result) { return false; }

    // If "waiting" packet, continue. Else, break.
    if (json_recv.at("type") == "waiting") {
      // Delay and continue
      std::this_thread::sleep_for(std::chrono::microseconds(rand() % 500));
    } else {
      assert(json_recv["type"] == "response");
      got_fix = true;
      break;
    }
  }

  // Transcribe fix data
  assert(json_recv.at("atoms").as_array().size() == _n);
  for (size_t i = 0; i < _n; ++i) { _into[i] = from_json(json_recv.at("atoms").as_array().at(i)); }

  return true;
}

uint send_registration(const int &_controller_rank)
{
  boost::mpi::communicator comm;
  boost::json::object json;
  std::random_device rng;
  std::uniform_int_distribution<uint> time_dist(0, 500);
  bool result;

  json["type"] = "register";
  comm.send(_controller_rank, 0, boost::json::serialize(json));

  json.clear();
  result = await_packet(1'000.0, json, rng, time_dist, _controller_rank);
  assert(result);

  // If packet is malformed, uid is zero (error state)
  if (!json.contains("type") || json.at("type") != "ack" || !json.contains("uid")) { return 0; }
  return json.at("uid").as_int64();
}

void send_deregistration(const uint &_id, const int &_controller_rank)
{
  boost::mpi::communicator comm;
  boost::json::object to_encode;
  std::string to_send;

  to_encode["type"] = "deregister";
  to_encode["uid"] = _id;
  to_send = boost::json::serialize(to_encode);
  comm.send(_controller_rank, 0, to_send);
}
