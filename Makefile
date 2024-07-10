CFLAGS = -Wall -Wextra -Werror
CSTD = -std=c11

generate-itermcolors: main.c
	$(CC) -o $@ $< $(CSTD) $(CFLAGS)
