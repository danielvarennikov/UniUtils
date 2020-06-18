# Tool invocations

all: myshell mypipeline

myshell: myshell.o LineParser.o looper.o
	gcc -g -Wall -m32 myshell.o LineParser.o -o myshell

myshell.o: myshell.c
	gcc -g -Wall -m32 -c -o myshell.o myshell.c

LineParser.o: LineParser.c
	gcc -g -Wall -m32 -c -o LineParser.o LineParser.c

mypipeline: mypipeline.o
	gcc -g -Wall -m32 mypipeline.o -o mypipeline

mypipeline.o: mypipeline.c
	gcc -g -Wall -m32 -c -o mypipeline.o mypipeline.c



.PHONY : clean
clean :
	-rm -f *.o
