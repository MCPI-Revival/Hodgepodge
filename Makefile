INCLUDES = -isystem ~/.minecraft-pi/sdk/lib/minecraft-pi-reborn-client/sdk/include/libreborn -isystem ~/.minecraft-pi/sdk/lib/minecraft-pi-reborn-client/sdk/include/symbols -isystem ~/.minecraft-pi/sdk/lib/minecraft-pi-reborn-client/sdk/include/mods -isystem ~/.minecraft-pi/sdk/lib/minecraft-pi-reborn-client/sdk/include/media-layer
CPP_FLAGS = -shared -fPIC -std=c++17 -Wall -Wextra -DREBORN_HAS_PATCH_CODE -D_GLIBCXX_USE_CXX11_ABI=0 -D__FILE__="\"libNOLEAKING.so\"" -Wno-builtin-macro-redefined $(INCLUDES)
LD_FLAGS = -shared -fPIC -Wl,-rpath,~/.minecraft-pi/sdk/lib/minecraft-pi-reborn-client/sdk/lib ~/.minecraft-pi/sdk/lib/minecraft-pi-reborn-client/sdk/lib/lib*.so
CC = arm-linux-gnueabihf-g++

SRC = ${wildcard ./src/*.cpp}
OBJ = ${patsubst ./src/%.cpp,build/%.o,${SRC}}

all: build
build: $(OBJ)
	$(CC) $(LD_FLAGS) $(OBJ) -o libNOLEAKING.so

build/%.o: ./src/%.cpp
	$(CC) $(CPP_FLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) libNOLEAKING.so
