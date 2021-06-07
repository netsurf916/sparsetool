all: sparsetool.cpp sparse.cpp sparse.h
	g++ -I./ -o sparsetool sparse.cpp sparsetool.cpp

clean:
	-rm sparsetool

