CFLAG= -std=c++11 -I ../../build/release-install-cpp11/include/
LFLAG= -L../../build/release-install-cpp11/lib -ljsoncpp -lmuduo_net -lmuduo_base -pthread 
all: server client
servert: test_server.cc
	g++  $(CFLAG) $^ -o $@ $(LFLAG)
clientt: test_client.cc
	g++  $(CFLAG) $^ -o $@ $(LFLAG)

clean:
	rm -rf server client

# test:test.cc
# 	g++ $^ -o $@ -std=c++11 -ljsoncpp
# clean:
# 	rm -rf test

# CFLAG= -std=c++11 -I ../../build/release-install-cpp11/include/
# LFLAG= -L../../build/release-install-cpp11/lib -ljsoncpp -lmuduo_net -lmuduo_base -pthread 
# all: server
# server: test_server.cc
# 	g++  $(CFLAG) $^ -o $@ $(LFLAG)

# clean:
# 	rm -rf server