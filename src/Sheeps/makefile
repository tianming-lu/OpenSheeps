INC=-I./../third
LIB=-pthread -L./../third -lthird
TARGET=libsheeps.so
OBJ=console.o EpollReactor.o SheepsMain.o pch.o ServerConsole.o ServerProtocol.o ServerProxy.o ServerStress.o ClientProtocol.o SheepsFactory.o ClientProtocolSub.o TaskManager.o

CC=gcc
CXX=g++ -std=c++11
CFLAGS=-g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64 -fPIC

ALL:$(TARGET)
#ld -r -b binary -o console.html.o console.html
$(TARGET):$(OBJ)
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LIB)
%.o: %.cpp
	$(CXX) $(CFLAGS) $(INC) -c $< 	 
%.o: %.c
	$(CC) $(CFLAGS) $(INC) -c $< 	 
%.o: %.html
	ld -r -b binary -o $@ $< 
clean:
	rm -f *.o *.so
	rm -f $(TARGET) 	