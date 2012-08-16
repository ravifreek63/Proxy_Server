all: server sim
	
server: md5.h cache.h md5.cpp cache.cpp Ravi2.cpp    
	g++ -o server md5.cpp cache.cpp Ravi2.cpp  -lpthread -std=gnu++0x
	
sim: md5.h cache.h md5.cpp cache.cpp Sim.cpp    
	g++ -o simulator md5.cpp cache.cpp Sim.cpp -lpthread -std=gnu++0x
	
