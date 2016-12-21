benchmark: new.o src.o
	gcc -o benchmark new.o src.o -pthread -Wall
	rm -f *~ *.o *.h.gch

new.o:	new.c src.h	
	gcc -c new.c src.h -pthread -Wall

src.o:	src.c src.h
	gcc -c src.c -Wall
