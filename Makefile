all:
	gcc csmc.c -o csmc -Wall -Wextra -Werror -pthread -std=c11
clean:
	rm csmc
