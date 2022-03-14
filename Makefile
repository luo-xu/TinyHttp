all: tinyhttp

tinyhttp: httpConn.cpp main.cpp
	g++ httpConn.cpp main.cpp -o tinyhttp  -lpthread 

clean:
	rm tinyhttp
