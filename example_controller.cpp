/*
A C++ example controller. This is trivially adaptable for
arbitrary C++ controllers: Just make sure you start the
controller with the rest of the LAMMPS MPI jobs!
*/

#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serializer.hpp>
#include <boost/mpi.hpp>
#include <boost/mpi/communicator.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <list>
#include <mpi.h>
#include <set>
#include <string>

int main()
{
  std::set<uint> uids;
  std::list<uint> open_uids = {1};
  boost::json::object json;
  std::string message;
  boost::mpi::communicator comm;
  int world_rank;

  MPI_Init(NULL, NULL);
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  std::cout << __FILE__ << ":" << __LINE__ << "> " << "Starting controller\n";

  // For as long as there are connections left
  do {
    std::cout << __FILE__ << ":" << __LINE__ << "> " << "Waiting for packet\n";

    // Await some packet
    const auto status = comm.recv(boost::mpi::any_source, boost::mpi::any_tag, message);
    json = boost::json::parse(message).as_object();

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

      comm.send(status.source(), status.tag(), boost::json::serialize(json));
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
      comm.send(status.source(), status.tag(), boost::json::serialize(json_to_send));
    }
  } while (!uids.empty());

  std::cout << __FILE__ << ":" << __LINE__ << "> " << "Halting controller\n";
  MPI_Finalize();

  return 0;
}
