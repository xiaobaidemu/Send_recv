CC = g++
SRCS = net.cpp test.cpp
OPTS = -m64 -std=c++11 -w
OBJS = $(SRCS:.cpp=.o)
LIB = -lpthread
EXEC = test

%.o : %.cpp
	$(CC) $(OPTS) -c $< -o $@
 
start:$(OBJS)
	$(CC) $(OPTS) $(OBJS) -o $(EXEC) $(LIB)

clean:
	rm -f test
	rm -f $(OBJS)
