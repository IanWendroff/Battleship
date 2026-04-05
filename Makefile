CXX = g++
CXXFLAGS = -std=c++17 -Wall

all: host join

host: host_lobby.cpp
	$(CXX) $(CXXFLAGS) -o host host_lobby.cpp

join: join_lobby.cpp
	$(CXX) $(CXXFLAGS) -o join join_lobby.cpp

clean:
	rm -f host join