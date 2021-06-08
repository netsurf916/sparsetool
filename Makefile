all: sparsetool.cpp sparse.cpp sparse.hpp
	g++ -I./ -o sparsetool sparse.cpp sparsetool.cpp

clean:
	-rm sparsetool

