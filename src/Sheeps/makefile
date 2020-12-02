INC=
LIB=-pthread -L./../third -lthird
TARGET=libsheeps.so
OBJ=ClientProtocol.o ClientProtocolSub.o EpollReactor.o pch.o ServerConsole.o ServerProtocol.o ServerProxy.o ServerStress.o SheepsFactory.o SheepsMain.o SheepsMemory.o TaskManager.o

CC=gcc
CXX=g++ -std=c++11
CFLAGS=-g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64 -fPIC

ALL:$(TARGET)
$(TARGET):$(OBJ)
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LIB)
%.o: %.cpp
	$(CXX) $(CFLAGS) $(INC) -c $< 	 
%.o: %.c
	$(CC) $(CFLAGS) $(INC) -c $< 	 
clean:
	rm -f *.o *.so
	rm -f $(TARGET) 	