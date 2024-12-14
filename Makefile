CPP := mpicxx -pedantic -Wall -g -O3
LIBS := -lboost_mpi -lboost_json -lboost_serialization

.PHONY:	all
all:	example_controller.out example_worker.out

%.o:	%.cpp
	$(CPP) -c -o $@ $<

%.out:	%.o ARBFN/interchange.o
	$(CPP) -o $@ $^ $(LIBS)

ARBFN/interchange.o:	ARBFN/interchange.cpp ARBFN/interchange.hpp
	$(MAKE) -C ARBFN interchange.o

.PHONY:	format
format:
	find . -type f \( -iname "*.cpp" -or -iname "*.hpp" \) \
		-exec clang-format -i "{}" \;

.PHONY:	test
test:	all
	mpirun -n 1 ./example_controller.out : -n 3 ./example_worker.out

.PHONY:	clean
clean:
	find . -type f \( -iname '*.o' -or -iname '*.out' -or -iname '*.so' \) -exec rm -f "{}" \;
