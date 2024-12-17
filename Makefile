CPP := mpicxx -pedantic -Wall -g -O3 -std=c++17
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
	find . -type f -iname "*.py" -exec \
		autopep8 --in-place --aggressive --aggressive "{}" \;

.PHONY:	test
test:	test1 test2

.PHONY:	test1
test1:	example_controller.out example_worker.out
	mpirun -n 1 ./example_controller.out : -n 3 ./example_worker.out

.PHONY:	test2
test2:	example_controller_2.py example_worker.out
	mpirun -n 1 ./example_controller_2.py : -n 3 ./example_worker.out

.PHONY:	clean
clean:
	find . -type f \( -iname '*.o' -or -iname '*.out' -or -iname '*.so' \) -exec rm -f "{}" \;
