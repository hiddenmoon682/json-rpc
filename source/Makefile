CFLAG= -std=c++11 -I ../../build/release-install-cpp11/include/
LFLAG= -L../../build/release-install-cpp11/lib -ljsoncpp -lmuduo_net -lmuduo_base -pthread 
all : test_server test_publish_client test_subscribe_client
test_server : server.cc
	g++  -g $(CFLAG) $^ -o $@ $(LFLAG)
test_publish_client : publish_client.cc
	g++  -g $(CFLAG) $^ -o $@ $(LFLAG)
test_subscribe_client : subscribe_client.cc
	g++  -g $(CFLAG) $^ -o $@ $(LFLAG)

clean:
	rm -rf test_server test_publish_client test_subscribe_client

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