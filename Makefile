# MakeFile for anonymous proxy
# Filipe <mail@filipe.pt>

CC = gcc
CFLAGS = -lpthread -v
xFlag = -lnsl  

all: proxy

extra_utils.o:extra_utils.c
	$(CC) -c extra_utils.c

utils.o:utils.c
	$(CC) -c utils.c

proxy.o:proxy.c
	$(CC) -c proxy.c

proxy: proxy.o utils.o extra_utils.o 
	$(CC) -o proxy proxy.o utils.o extra_utils.o $(CFLAGS) $(xFlag)

clean:
	rm *.o
