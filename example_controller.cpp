/*
A C++ example controller. This is trivially adaptable for
arbitrary C++ controllers: Just make sure you start the
controller with the rest of the LAMMPS MPI jobs!
*/

#include <boost/json/src.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <list>
#include <mpi.h>
#include <set>
#include <string>

void send_json(MPI_Comm &_comm, const int &_target, const int &_tag,
               const boost::json::value &_what)
{
  std::stringstream s;
  std::string raw;

  s << _what;
  raw = s.str() + "\0";
  MPI_Send(raw.c_str(), raw.size(), MPI_CHAR, _target, _tag, _comm);
}

MPI_Status recv_json(MPI_Comm &_comm, const int &_source, boost::json::value &_into)
{
  MPI_Status out;
  char *buffer;

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
  std::set<uint> uids;
  std::list<uint> open_uids = {1};
  boost::json::object json;
  boost::json::value json_val;
  std::string message;
  MPI_Comm comm = MPI_COMM_WORLD;
  int world_rank;

  MPI_Init(NULL, NULL);
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  std::cout << __FILE__ << ":" << __LINE__ << "> " << "Starting controller\n";

  // For as long as there are connections left
  do {
    std::cout << __FILE__ << ":" << __LINE__ << "> " << "Waiting for packet\n";

    // Await some packet
    const auto status = recv_json(comm, MPI_ANY_SOURCE, json_val);
    json = json_val.as_object();

    std::cout << __FILE__ << ":" << __LINE__ << "> " << "Got message w/ type " << json["type"]
              << "\n";

    assert(json["type"] != "waiting" && json["type"] != "ack" && json["type"] != "response");

    // Book keeping
    if (json["type"] == "register") {
      // Register a new worker

      json.clear();
      json["type"] = "ack";

      if (open_uids.empty()) { open_uids.push_back(uids.size() + 1); }
      json["uid"] = open_uids.front();
      uids.insert(open_uids.front());
      open_uids.pop_front();

      json_val = json;
      send_json(comm, status.MPI_SOURCE, status.MPI_TAG, json_val);
    }

    else if (json["type"] == "deregister") {
      // Erase a worker
      uint64_t uid = json.at("uid").as_int64();
      assert(uids.count(uid) != 0);
      uids.erase(uid);
      open_uids.push_back(uid);
    }

    // Data processing
    else if (json["type"] == "request") {
      // Determine fix to send back
      boost::json::array list;
      for (const auto &item : json["atoms"].as_array()) {
        boost::json::object fix;
        fix["dfx"] = -0.5 * item.at("fx").as_double();
        fix["dfy"] = -0.5 * item.at("fy").as_double();
        fix["dfz"] = -0.5 * item.at("fz").as_double();
        list.push_back(fix);
      }

      boost::json::object json_to_send;
      json_to_send["type"] = "response";
      json_to_send["atoms"] = list;
      json_to_send["uid"] = json["uid"];
      assert(json["atoms"].as_array().size() == json_to_send["atoms"].as_array().size());

      // Send fix data back
      send_json(comm, status.MPI_SOURCE, status.MPI_TAG, json_to_send);
    }
  } while (!uids.empty());

  std::cout << __FILE__ << ":" << __LINE__ << "> " << "Halting controller\n";
  MPI_Finalize();

  return 0;
}
