# Tool invocations

all: myElf 

myElf: myElf.o
	gcc -g -Wall -m32 myElf.o -o myElf

myElf.o: myElf.c
	gcc -g -Wall -m32 -c -o myElf.o myElf.c



.PHONY : clean
clean :
	-rm -f *.o
