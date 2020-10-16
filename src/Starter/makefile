NS=starter
INC=
LIB= -ldl -rdynamic  -pthread

OBJ= Starter.o

CC=gcc
CXX=g++ -std=c++11
TARGET=$(NS)
DEFINE=-D_GNU_SOURCE -D_REENTRANT -D_NS_=\"$(NS)\"
CFLAGS=-g -O2 -pipe -Wall -fno-strict-aliasing -Wno-sizeof-pointer-memaccess
CFLAGS+=$(DEFINE)

all:$(TARGET)
$(TARGET):$(OBJ)
	$(CXX) $(CFLAGS) -o $@ $^ $(LIB)
%.o: %.cpp
	$(CXX) $(CFLAGS) $(INC) -c $<
%.o: %.c
	$(CC) $(CFLAGS) $(INC) -c $<
clean:
	rm -f $(OBJ)
	rm -f $(TARGET)