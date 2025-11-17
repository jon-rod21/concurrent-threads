all:
	gcc csmc.c -o csmc -Wall -Wextra -Werror -pthread -std=gnu11
clean:
	rm csmc
