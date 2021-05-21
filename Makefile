driver: driver.cpp
	g++ -fopenmp ./driver.cpp -g -o driver -O1 -I/home/pvelesko/Downloads/pcm -L/home/pvelesko/Downloads/pcm -lpcm -Wl,-rpath=/home/pvelesko/Downloads/pcm
	sudo setcap cap_sys_rawio=ep ./driver

clean:
	rm ./driver
