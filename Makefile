all: main
.PHONY : all

main: main.c
	gcc -Wall -Wextra -g -pthread main.c -o worker_dispatcher


.PHONY : clean
clean:
	\rm worker_dispatcher || true
	\rm thread*.txt || true
	\rm counter*.txt || true
	\rm file_stats.txt || true
	\rm dispatcher.txt || true


tidycode:
	clang-format -i *.c
