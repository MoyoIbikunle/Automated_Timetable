# Windows: run from the w64devkit terminal
# Raylib expected at C:\raylib (include\raylib.h and lib\libraylib.a)
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -IC:/raylib/include
LIBS = -LC:/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm
LOGIC = data.c scheduler.c

rota.exe: main.c $(LOGIC) rota.h
	$(CC) $(CFLAGS) main.c $(LOGIC) -o rota.exe $(LIBS)

# Console test of the loader and generator (no Raylib needed)
test: test_rota.c $(LOGIC) rota.h
	$(CC) -std=c99 -Wall -Wextra test_rota.c $(LOGIC) -o test_rota.exe
	./test_rota.exe

clean:
	rm -f rota.exe test_rota.exe
