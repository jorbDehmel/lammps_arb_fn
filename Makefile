CPP := mpicxx -O3 -std=c++11
LIBS := ARBFN/interchange.o

.PHONY:	check
check:
	@echo "Checking for libboost-json and mpicxx w/ C++11..."
	@$(CPP) example_controller.cpp -c -o /dev/null

	@echo "Checking for python3..."
	@python3 --version > /dev/null

	@echo "Checking for pip..."
	@pip --version > /dev/null

	@echo "Checking for mpi4py..."
	@pip list | grep mpi4py > /dev/null

	@echo "Checking for autopep8..."
	@pip list | grep autopep8 > /dev/null

	@echo "Checking for cmake..."
	@cmake --version > /dev/null

	@echo "Checking for clang-format..."
	@clang-format --version > /dev/null

	@echo "Environment is valid."

.PHONY:	format
format:
	find . -type f \( -iname "*.cpp" -or -iname "*.hpp" \) \
		-exec clang-format -i "{}" \;
	find . -type f -iname "*.py" -exec \
		autopep8 --in-place --aggressive --aggressive "{}" \;

.PHONY:	test
test:	test1 test2 test3

.PHONY:	test1
test1:
	$(MAKE) -C tests $@

.PHONY:	test2
test2:
	$(MAKE) -C tests $@

.PHONY:	test3
test3:
	$(MAKE) -C tests $@

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
