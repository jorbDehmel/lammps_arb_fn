CPP := mpicxx -O3 -std=c++11
LIBS := ARBFN/interchange.o
EXTRA ?=

%.o:	%.cpp
	$(CPP) -c -o $@ $^ $(EXTRA)

example_controller.out:	example_controller.o
	$(CPP) -o $@ $^

%.out:	%.o $(LIBS)
	$(CPP) -o $@ $^

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
	mpirun --map-by :OVERSUBSCRIBE -n 1 ./example_controller.out : --map-by :OVERSUBSCRIBE -n 3 ./example_worker.out

.PHONY:	test2
test2:	example_controller_2.py example_worker.out
	mpirun --map-by :OVERSUBSCRIBE -n 1 ./example_controller_2.py : --map-by :OVERSUBSCRIBE -n 3 ./example_worker.out

.PHONY:	clean
clean:
	find . -type f \( -iname '*.o' -or -iname '*.out' -or -iname '*.so' \) -exec rm -f "{}" \;

################################################################
# Docker launching stuff
################################################################

# For absolute path usage later
cwd := $(shell pwd)

# Enter into the docker container
.PHONY: launch-docker
launch-docker:	| build Makefile
	docker run \
		--privileged \
		--mount type=bind,source="/dev/",target="/dev/" \
		--mount type=bind,source="${cwd}",target="/host" \
		-i \
		-t arbfn:latest \

# Build the docker container from ./Dockerfile
.PHONY:	build
build:	| Makefile
	docker build --tag 'arbfn' .
