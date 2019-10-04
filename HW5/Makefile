FLAGS=-c -Wall -ansi -std=gnu99 -pedantic -errors

all: Banka Client
	
Banka: Banka.o
	gcc Banka.o -o Banka
Banka.o: Banka.c
	gcc $(FLAGS) Banka.c

Client: Client.o
	gcc Client.o -o Client
Client.o: Client.c
	gcc $(FLAGS) Client.c



clean:
	rm -rf *.o Banka Client
