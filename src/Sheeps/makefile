INC=-I./../third
LIB=-pthread -L./../third -lthird
TARGET=libsheeps.so
OBJ=favicon.o jquery_min.o console.o EpollReactor.o SheepsMain.o pch.o ServerConsole.o ServerProtocol.o ServerProxy.o ServerStress.o ClientProtocol.o SheepsFactory.o ClientProtocolSub.o TaskManager.o

CC=gcc
CXX=g++ -std=c++11
CFLAGS=-g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64 -fPIC


all: clearfile copyfile
	make build

clearfile:
	rm -f *.html *.js *.ico

copyfile:
	cp ../../webConsole/console.html ./
	cp ../../webConsole/jquery.min.js ./jquery_min.js
	cp ../../webConsole/favicon.ico ./

build: $(TARGET)

$(TARGET):$(OBJ)
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LIB)
%.o: %.cpp
	$(CXX) $(CFLAGS) $(INC) -c $<
%.o: %.c
	$(CC) $(CFLAGS) $(INC) -c $<
%.o: %.html
	ld -r -b binary -o $@ $<
%.o: %.js
	ld -r -b binary -o $@ $<
%.o: %.ico
	ld -r -b binary -o $@ $<

clean:
	rm -f *.o *.so *.html *.js *.ico
	rm -f $(TARGET)