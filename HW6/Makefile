FLAGS=-c -Wall -ansi -std=gnu99 -pedantic -errors
  
pCp: pCp.o
	gcc pCp.o -o pCp -pthread
pCp.o: pCp.c
	gcc $(FLAGS) pCp.c

clean:
	rm -rf *.o pCp
