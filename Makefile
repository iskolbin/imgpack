all:
	cc -std=c99 -Wall -Wextra -Wshadow -O3 main.c -o imgpack -lm

pack:
	cc -E main.c > imgpack.c

pack-zip:
	cc -E main.c > imgpack.c
	zip -9 imgpack.zip imgpack.c
