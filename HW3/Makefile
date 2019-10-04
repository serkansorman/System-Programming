FLAGS=-c -Wall -ansi -std=gnu99 -pedantic -errors

all: lsf pwd wc cat bunedu gtushell 

lsf: lsf.o
	gcc lsf.o -o lsf

lsf.o: lsf.c
	gcc $(FLAGS) lsf.c


pwd: pwd.o
	gcc pwd.o -o pwd

pwd.o: pwd.c
	gcc $(FLAGS) pwd.c

wc: wc.o
	gcc wc.o -o wc

wc.o: wc.c
	gcc $(FLAGS) wc.c


cat: cat.o
	gcc cat.o -o cat

cat.o: cat.c
	gcc $(FLAGS) cat.c


bunedu: bunedu.o
	gcc bunedu.o -o bunedu

bunedu.o: bunedu.c
	gcc $(FLAGS) bunedu.c

gtushell: gtushell.o
	gcc gtushell.o -o gtushell

gtushell.o: gtushell.c
	gcc $(FLAGS) gtushell.c

clean:
	rm -rf *.o gtushell lsf pwd wc cat bunedu
