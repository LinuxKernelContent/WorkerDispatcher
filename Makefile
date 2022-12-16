all: main
.PHONY : all

main: main.c
	gcc -Wall -Wextra -g -pthread *.c -o main


.PHONY : clean
clean:
	\rm main 
	\rm counter*


.PHONY : testmem
testmem:
	valgrind --leak-check=full --quiet --show-leak-kinds=all --track-origins=yes --verbose ./main cmdfile.txt 5 5 1

tidycode:
	clang-format -i *.c

run :
	./main cmdfile.txt 6 10 1