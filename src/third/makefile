INC=
LIB=
OBJ=cJSON.o common.o log.o md5.o mycrypto.o sha1.o sqlite3.o Config.o
TARGET=libthird.a

CC=gcc
CXX=g++
DEFINE=-D_GNU_SOURCE -D_REENTRANT
CFLAGS=-g -O2 -pipe -Wall -fno-strict-aliasing -Wno-sizeof-pointer-memaccess -fPIC
CFLAGS+=$(DEFINE)
CFLAGS+=$(INC)
CFLAGS+=$(LIB)

all:$(TARGET)
$(TARGET):$(OBJ)
	ar rcs $(TARGET) $(OBJ)
%.o:%.cpp
	$(CXX) $(CFLAGS) -c $<
%.o:%.c
	$(CC) $(CFLAGS) -c $<
clean:
	rm -rf $(TARGET) *.o
