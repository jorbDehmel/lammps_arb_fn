FROM ubuntu:latest

RUN apt-get update
RUN apt-get install -y build-essential git g++ python3 python3-pip libopenmpi-dev libboost-all-dev
RUN git clone -b release https://github.com/lammps/lammps.git /home/lammps
RUN apt-get install -y nano
RUN make -C /home/lammps/src mpi -j 8
WORKDIR /host
