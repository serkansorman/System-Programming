FLAGS=-c -Wall -ansi -std=gnu99 -pedantic -errors

buNeDuFork: buNeDuFork.o
	gcc buNeDuFork.o -o buNeDuFork

buNeDuFork.o: buNeDuFork.c
	gcc $(FLAGS) buNeDuFork.c

clean:
	rm *.o buNeDuFork
