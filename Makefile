CC = gcc
CXXC = g++
CFLAGS = -Wall -O3 -g

all: svm-train svm-classify

svm-classify: svm-classify.c svm.o
	$(CC) $(CFLAGS) svm-classify.c svm.o -o svm-classify -lm
svm-train: svm-train.c svm.o
	$(CC) $(CFLAGS) svm-train.c svm.o -o svm-train -lm
svm.o: svm.cpp svm.h
	$(CXXC) $(CFLAGS) -c svm.cpp
scale: scale.c
	$(CC) $(CFLAGS) scale.c -o scale
commit:
	LOGNAME=adm cvs commit
clean:
	rm -f *~ svm.o svm-train svm-classify scale
