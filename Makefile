CC = gcc
CXXC = g++
CFLAGS = -Wall -O3 -g

all: svm-train svm-predict

svm-predict: svm-predict.c svm.o
	$(CC) $(CFLAGS) svm-predict.c svm.o -o svm-predict -lm
svm-train: svm-train.c svm.o
	$(CC) $(CFLAGS) svm-train.c svm.o -o svm-train -lm
svm.o: svm.cpp svm.h
	$(CXXC) $(CFLAGS) -c svm.cpp
scale: scale.c
	$(CC) $(CFLAGS) scale.c -o scale
commit:
	LOGNAME=adm cvs commit
clean:
	rm -f *~ svm.o svm-train svm-predict scale
