CPP := g++ -pedantic -Wall

.PHONY:	all
all:	example_controller.out

%.o:	%.cpp
	$(CPP) -c -o $@ $<

%.out:	%.o
	$(CPP) -o $@ $<

.PHONY:	format
format:
	find . -type f \( -iname "*.cpp" -or -iname "*.hpp" \) \
		-exec clang-format -i "{}" \;

.PHONY:	clean
clean:
	rm -f *.o *.out
