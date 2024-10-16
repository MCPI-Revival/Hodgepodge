INCLUDES = -isystem ~/.minecraft-pi/sdk/lib/minecraft-pi-reborn-client/sdk/include/libreborn -isystem ~/.minecraft-pi/sdk/lib/minecraft-pi-reborn-client/sdk/include/symbols -isystem ~/.minecraft-pi/sdk/lib/minecraft-pi-reborn-client/sdk/include/mods -isystem ~/.minecraft-pi/sdk/lib/minecraft-pi-reborn-client/sdk/include/media-layer
CPP_FLAGS = -g -shared -fPIC -std=c++17 -Wall -Wextra -DREBORN_HAS_PATCH_CODE -D_GLIBCXX_USE_CXX11_ABI=0 $(INCLUDES)
LD_FLAGS = -shared -g -fPIC -Wl,-rpath,~/.minecraft-pi/sdk/lib/minecraft-pi-reborn-client/sdk/lib ~/.minecraft-pi/sdk/lib/minecraft-pi-reborn-client/sdk/lib/lib*.so
CC = arm-linux-gnueabihf-g++

SRC = ${wildcard ./src/*.cpp}
OBJ = ${patsubst ./src/%.cpp,build/%.o,${SRC}}

all: build
build: $(OBJ)
	$(CC) $(LD_FLAGS) $(OBJ) -o libredstone.so

build/%.o: ./src/%.cpp
	$(CC) -D__FILE__="\"libredstone.so:$<\"" -Wno-builtin-macro-redefined $(CPP_FLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) libredstone.so


FORCE: ;
cmake: FORCE
	mkdir -p cmake
	cd cmake; \
	 cmake ..; \
	 make
