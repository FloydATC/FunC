
CC = /usr/bin/gcc
LINKER = /usr/bin/gcc
SRC = 
OBJ = obj/Debug/
BIN = bin/Debug/
CINC = 
LINC = -lm
clean:
	rm -f /obj/Debug
	rm -f /bin/Debug

debugdeps:
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)chunk.c -o $(OBJ)chunk.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)compiler.c -o $(OBJ)compiler.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)debug.c -o $(OBJ)debug.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)error.c -o $(OBJ)error.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)file.c -o $(OBJ)file.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)index.c -o $(OBJ)index.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)main.c -o $(OBJ)main.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)memory.c -o $(OBJ)memory.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)objarray.c -o $(OBJ)objarray.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)object.c -o $(OBJ)object.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)objstring.c -o $(OBJ)objstring.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)scanner.c -o $(OBJ)scanner.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)table.c -o $(OBJ)table.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)value.c -o $(OBJ)value.o
	$(CC) -Wall -g -DDEBUG $(CINC) -c $(SRC)vm.c -o $(OBJ)vm.o

debug:
	$(LINKER) -o $(BIN)FunC $(OBJ)chunk.o $(OBJ)compiler.o $(OBJ)debug.o $(OBJ)error.o $(OBJ)file.o $(OBJ)index.o $(OBJ)main.o $(OBJ)memory.o $(OBJ)objarray.o $(OBJ)object.o $(OBJ)objstring.o $(OBJ)scanner.o $(OBJ)table.o $(OBJ)value.o $(OBJ)vm.o $(LINC)
 
