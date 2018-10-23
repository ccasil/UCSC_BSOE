# https://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

CC = cc
CFLAGS = -Wall -g
CSOURCES = argshell.c lex.yy.c
OBJS = $(CSOURCES:.c=.o)
MAIN = argshell

all: $(MAIN)

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) -o $(MAIN) $(OBJS) -lfl

argshell.o: argshell.c
	$(CC) -c argshell.c

lex.yy.o: lex.yy.c
	$(CC) -c lex.yy.c

lex.yy.c: shell.l
	flex shell.l

clean:
	rm -f $(MAIN) lex.yy.c $(OBJS) argshell.core
