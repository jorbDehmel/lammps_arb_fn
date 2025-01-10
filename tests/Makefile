CPP := mpicxx -O3 -std=c++11
LIBS := ../ARBFN/interchange.o

%.o:	%.cpp
	$(CPP) -c -o $@ $^ $(EXTRA)

%.out:	%.o
	$(CPP) -o $@ $^

example_worker.out:	example_worker.o $(LIBS)
	$(CPP) -o $@ $^

.PHONY:	format
format:
	find . -type f \( -iname "*.cpp" -or -iname "*.hpp" \) \
		-exec clang-format -i "{}" \;
	find . -type f -iname "*.py" -exec \
		autopep8 --in-place --aggressive --aggressive "{}" \;

.PHONY:	test
test:	test1 test2 test3

.PHONY:	test1
test1:	example_controller.out example_worker.out
	mpirun --map-by :OVERSUBSCRIBE -n 1 \
		./example_controller.out \
		: --map-by :OVERSUBSCRIBE -n 3 \
		./example_worker.out

.PHONY:	test2
test2:	example_controller_2.py example_worker.out
	mpirun --map-by :OVERSUBSCRIBE -n 1 \
		./example_controller_2.py \
		: --map-by :OVERSUBSCRIBE -n 3 \
		./example_worker.out

.PHONY:	test3
test3:	example_bulk_controller.out example_worker.out
	mpirun --map-by :OVERSUBSCRIBE -n 1 \
		./example_bulk_controller.out \
		: --map-by :OVERSUBSCRIBE -n 3 \
		./example_worker.out

.PHONY:	clean
clean:
	find . -type f \( -iname '*.o' -or -iname '*.out' -or \
		-iname '*.so' \) -exec rm -f "{}" \;
