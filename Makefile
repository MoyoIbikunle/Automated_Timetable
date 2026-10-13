# Windows: run from the w64devkit terminal
# Raylib expected at C:\raylib (include\raylib.h and lib\libraylib.a)
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -IC:/raylib/include
LIBS = -LC:/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm
SRC = main.c

rota.exe: $(SRC) rota.h
	$(CC) $(CFLAGS) $(SRC) -o rota.exe $(LIBS)

clean:
	rm -f rota.exe