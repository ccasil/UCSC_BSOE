CC = cc
CSOURCES = benchmark.c
MAIN = benchmark

all: $(MAIN)

$(MAIN): $(CSOURCES)
	$(CC) -o $(MAIN) $(CSOURCES) $(CFLAGS)

clean:
	rm -f $(MAIN) *.core *.log
