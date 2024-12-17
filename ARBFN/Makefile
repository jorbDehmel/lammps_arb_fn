# 2024, J Dehmel, J Schiffbauer

CPP := mpicxx -g -O3 -pedantic -Wall -shared -fPIC -std=c++11
HEADERS := fix_arbfn.hpp interchange.hpp
LIBS := -lboost_mpi -lboost_json -lboost_serialization

arbfn.so:	fix_arbfn.o interchange.o
	$(CPP) $^ -o $@ $(LIBS)

%.o:	%.cpp $(HEADERS)
	$(CPP) -c -o $@ $<

.PHONY:	clean
clean:
	rm -f *.o *.so
