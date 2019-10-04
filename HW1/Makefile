FLAGS=-c -Wall -ansi -std=gnu99 -pedantic -errors

buNeDu: buNeDu.o
	gcc buNeDu.o -o buNeDu

buNeDu.o: buNeDu.c
	gcc $(FLAGS) buNeDu.c

clean:
	rm *.o buNeDu
