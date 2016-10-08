all: par-shell par-shell-terminal

par-shell-terminal: par-shell-terminal.o
		gcc -pthread -Wall -o par-shell-terminal par-shell-terminal.o

par-shell-terminal.o: client.c
	gcc -Wall -g -c -o par-shell-terminal.o client.c

par-shell: projecto.o commandlinereader.o list.o
	gcc -pthread -Wall -o par-shell commandlinereader.o projecto.o list.o

projecto.o: projecto.c commandlinereader.c list.c
	gcc -Wall -g -c projecto.c

commandlinereader.o: commandlinereader.c commandlinereader.h 
	gcc -Wall -g -c commandlinereader.c

list.o: list.c list.h
	gcc -Wall -g -c list.c

clean: 
	rm -f *.o core
	rm -f par-shell
	rm -f par-shell-terminal
	rm -f `find . -name "par-shell-out-*.txt"`
	rm -f log.txt