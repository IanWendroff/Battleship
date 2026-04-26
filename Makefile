CXX = g++
CXXFLAGS = -std=c++17 -Wall -lpthread

all: host join

host: host_lobby.cpp node.cpp game.cpp
	$(CXX) $(CXXFLAGS) -o host host_lobby.cpp node.cpp game.cpp

join: join_lobby.cpp node.cpp game.cpp
	$(CXX) $(CXXFLAGS) -o join join_lobby.cpp node.cpp game.cpp

clean:
	rm -f host join