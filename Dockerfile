FROM ubuntu:latest

RUN apt-get update
RUN apt-get install -y build-essential git g++ python3 python3-pip libopenmpi-dev libboost-all-dev
RUN git clone -b release https://github.com/lammps/lammps.git /usr/lammps
WORKDIR /host
