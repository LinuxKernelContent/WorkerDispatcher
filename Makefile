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
	valgrind --quiet --leak-check=yes ./main cmdfile.txt 10 10 1
	valgrind --quiet --leak-check=yes ./main cmdfile.txt 5 5 1

tidycode:
	clang-format -i *.c