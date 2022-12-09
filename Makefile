main: main.c
	gcc -Wall -Wextra -g -pthread *.c -o main

clean:
	\rm main
	\rm counter*