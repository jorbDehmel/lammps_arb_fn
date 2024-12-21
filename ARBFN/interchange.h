/**
 * @brief Defines library functions for use in the ARBFN library
 * @author J Dehmel, J Schiffbauer, 2024, MIT License
 */

#ifndef ARBFN_INTERCHANGE_H
#define ARBFN_INTERCHANGE_H

#include <chrono>
#include <cstddef>
#include <mpi.h>
#include <random>
#include <thread>

/** @struct AtomData
 *  @brief Represents an atom
 *  @var AtomData::x The x-position of the particle
 *  @var AtomData::vx The x-velocity of the particle
 *  @var AtomData::fx The x-force of the particle
 *  @var AtomData::y The y-position of the particle
 *  @var AtomData::vy The y-velocity of the particle
 *  @var AtomData::fy The y-force of the particle
 *  @var AtomData::z The z-position of the particle
 *  @var AtomData::vz The z-velocity of the particle
 *  @var AtomData::fz The z-force of the particle
 */
struct AtomData {
  double x, vx, fx;
  double y, vy, fy;
  double z, vz, fz;
};

/** @struct FixData
 *  @brief Represents a fix on an atom
 *  @var FixData::dfx The delta to be added to fx
 *  @var FixData::dfy The delta to be added to fy
 *  @var FixData::dfz The delta to be added to fz
 */
struct FixData {
  double dfx, dfy, dfz;
};

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
                 const double &_max_ms, const uint &_controller_rank);

/**
 * @brief Sends a registration packet to the controller.
 * @return The UID associated with this worker, 0 on error.
 */
uint send_registration(uint &_controller_rank);

/**
 * @brief Sends a deregistration packet to the controller.
 * @param _id The UID granted to this worker at registration
 */
void send_deregistration(const uint &_id, const int &_controller_rank);

#endif
