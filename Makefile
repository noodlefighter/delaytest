

all: local

local:
	cmake --debug -H. -Bbuild -G "Unix Makefiles"
	cd build && make

clean:
	rm -rf build
