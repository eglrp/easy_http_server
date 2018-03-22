CXX=g++ -std=c++11
CXXFLAGS += -g -Wall
LDFLAGS += -lpthread

DEPS_INCLUDE_PATH=-I deps/json-cpp/include -I deps/http-parser/
DEPS_LIB_PATH=deps/json-cpp/libjson_libmt.a deps/http-parser/libhttp_parser.a
SRC_INCLUDE_PATH=-I src
SRC_LIB_PATH=src/libserver.a
OUTPUT_PATH=bin

objects := $(patsubst %.cpp, %.o, $(wildcard src/*.cpp))

all: deps prepare libserver.a test

prepare:
	mkdir -p bin
	cp deps/http-parser/http_parser.h deps/http-parser/http_parser.c src/
	cp -r deps/json-cpp/json src/

deps:
	make -C deps/http-parser/ package
	make -C deps/json-cpp/

libserver.a: $(objects)
	ar -crs src/libserver.a src/*.o

test: http_server

%.o:%.cpp
	$(CXX) -c $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(SRC_INCLUDE_PATH) $< -o $@

http_server: tests/test_http_server.cpp
	$(CXX) $(CXXFLAGS) $(DEPS_INLCUDE_PATH) $(SRC_INCLUDE_PATH) $< $(LDFLAGS) $(SRC_LIB_PATH) $(DEPS_LIB_PATH) -o bin/$@

clean:
	rm -rf bin
	rm -rf src/*.o src/*.a src/json src/http_parser.h src/http_parser.c

.PHONY: all 

