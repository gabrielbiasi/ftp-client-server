ftp: ftp-client.c ftp-server.c
	gcc -o ftp-client ftp-client.c -Wall -Wextra -pedantic
	gcc -o ftp-server ftp-server.c -Wall -Wextra -pedantic
	
clean:
	rm ftp-client *.o *~
	rm ftp-server *.o *~
