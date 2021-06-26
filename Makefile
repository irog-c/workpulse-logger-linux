CC = g++
CFILES = main.cpp
OUTFILE = prog.out
FLAGS = -std=c++17 -lX11 -lXmu

out:
	$(CC) $(FLAGS) $(CFILES) -o $(OUTFILE)
clean:
	rm $(OUTFILE)
