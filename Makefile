server: server.c
	gcc -g -pthread -o server server.c

clean:
	rm -f server