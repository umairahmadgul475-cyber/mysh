CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -pthread
SRCS = src/main.c src/parser.c src/executor.c src/builtins.c src/ai_bridge.c

mysh: $(SRCS) src/mysh.h
	$(CC) $(CFLAGS) $(SRCS) -o mysh -lreadline

clean:
	rm -f mysh
