CC = g++
TARGET = crypto

$(TARGET): main.o crypto.o
	$(CC) main.o crypto.o -lcrypto -o $(TARGET)

main.o: main.cpp
	$(CC) -c main.cpp

crypto.o: crypto.cpp
	$(CC) -c crypto.cpp

run: 
	./$(TARGET)

clean:
	rm -f *.out *.o $(TARGET)

