CC = gcc
CXXC = g++
CFLAGS = -Wall -O3 -g

svm-toy: svm-toy.cpp svm-toy.moc ../../svm.o
	$(CXXC) -I/usr/include/qt $(CFLAGS) svm-toy.cpp ../../svm.o -o svm-toy -lqt -L/usr/X11R6/lib -lX11
svm-toy.moc: svm-toy.cpp
	moc svm-toy.cpp -o svm-toy.moc
../../svm.o:
	cd ../..; make svm.o
clean:
	rm -f *~ svm-toy svm-toy.moc
