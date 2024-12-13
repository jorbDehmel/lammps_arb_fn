# 2024, J Dehmel, J Schiffbauer

CPP := g++ -pedantic -Wall
TARGETS := fix_arbfn.o
HEADERS := fix_arbfn.hpp

.PHONY:	all
all:	$(TARGETS)

%.o:	%.cpp $(HEADERS)
	$(CPP) -o $@ $<

.PHONY:	clean
clean:
	rm -f *.o
