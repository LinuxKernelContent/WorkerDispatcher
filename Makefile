main: main.c
	gcc -g -pthread *.c -o main

clean:
	\rm main
	\rm counter*