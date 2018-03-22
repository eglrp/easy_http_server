SRC_INCLUDE_PATH=-I src/lib_json

objects := $(patsubst %.cpp,%.o,$(wildcard src/lib_json/*.cpp))

all: libjson_libmt.a
	echo "make all"

libjson_libmt.a: $(objects)
	ar -rcs libjson_libmt.a src/lib_json/*.o

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(SRC_INCLUDE_PATH) $< -o $@

clean:
	rm -rf src/lib_json/*.o

.PHONY: all clean
