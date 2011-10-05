# Makes libipaddr.so

TARGET=libipaddr

libipaddr:	libipaddr.o
	gcc -shared libipaddr.o -ldl -o libipaddr.so

libipaddr.o:	libipaddr.c
	gcc -g -c  libipaddr.c -fPIC -DPIC -o libipaddr.o

clean:
	rm -f $(TARGET) core *.o *~ libipaddr.so
