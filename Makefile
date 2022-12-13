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
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./main cmdfile.txt 10 10 1
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./main cmdfile.txt 5 5 1

tidycode:
	clang-format -i *.c

run :
	./main cmdfile.txt 10 10 1