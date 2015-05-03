CC = gcc
CFLAGS = -Wall
INCLDIR = -Ilibtcod/include
EXTRLIB = libtcod/lib/libtcod-mingw.a

DOXYLOC = C:/Program Files/doxygen/bin/doxygen.exe

compile: obj/main.o
	$(CC) $(CFLAGS) obj/main.o -o bin/main $(EXTRLIB)

obj/main.o:
	$(CC) -c $(CFLAGS) $(INCLDIR) src/main.c -o obj/main.o

doxygen: src/main.c
	$(DOXYLOC) doxygen/doxyfile
	@echo.
	@echo Finished generating documentation in doxygen folder

clean:
	@rm -f -r doxygen/html
	@rm -f obj/main.o bin/main.exe

.PHONY: doxygen clean