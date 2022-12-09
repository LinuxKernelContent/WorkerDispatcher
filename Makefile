main: main.c
	gcc -Wall -Wextra -g *.c -o main

clean:
	\rm main
	\rm counter*