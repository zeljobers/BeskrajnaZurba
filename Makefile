CC = gcc
LDFLAGS = -lglut -lGL -lGLU -lm
main : main.c
	$(CC) $^ -o $@ $(LDFLAGS)
.PHONY : 
	brisi
brisi:
	rm -f main
