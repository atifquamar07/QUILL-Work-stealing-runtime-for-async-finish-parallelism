++all: quill-runtime.o libquill-runtime.so nqueens

quill-runtime.o: quill-runtime.cpp
	g++ -std=c++11 -fPIC -c quill-runtime.cpp -lpthread 

libquill-runtime.so: quill-runtime.o
	g++ -std=c++11 -shared -o libquill-runtime.so quill-runtime.o 

nqueens: nqueens.cpp
	g++ -std=c++11 nqueens.cpp -o nqueens -L. -lquill-runtime -lpthread 

clean:
	rm nqueens libquill-runtime.so quill-runtime.o 
