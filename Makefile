all: main
.PHONY : all

main: main.c
	gcc -O0 -Wall -Wextra -g -pthread *.c -o main


.PHONY : clean
clean:
	\rm main || true
	\rm thread*.txt || true
	\rm counter*.txt || true
	\rm file_stats.txt || true
	\rm dispatcher.txt || true


.PHONY : testmem
testmem:
	valgrind --quiet --leak-check=yes -track-origins=yes ./main cmdfile.txt 10 10 1


tidycode:
	clang-format -i *.c

run :
	./main cmdfile.txt 10 10 1