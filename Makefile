all: main
.PHONY : all

main: main.c
	gcc -Wall -Wextra -g -pthread *.c -o hw2


.PHONY : clean
clean:
	\rm hw2 || true
	\rm thread*.txt || true
	\rm counter*.txt || true
	\rm file_stats.txt || true
	\rm dispatcher.txt || true


tidycode:
	clang-format -i *.c
