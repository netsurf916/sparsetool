all: sparsetool.cpp sparse.cpp sparse.hpp
	$(CXX) $(CXXFLAGS) -I./ -o sparsetool sparse.cpp sparsetool.cpp

clean:
	-rm sparsetool

