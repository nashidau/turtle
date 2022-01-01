

all:
	mkdir -p build
	(cd build && cmake .. && make)

clean:
	(cd build && make clean)

real-clean:
	rm -rf build
