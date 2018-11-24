.PHONY: build run clean

build: editor

editor: editor.o
	gcc -Wall editor.o -o editor
editor.o: editor.c
	gcc -c editor.c -o editor.o
run: editor
	./editor
clean:
	rm -rf editor.o editor
