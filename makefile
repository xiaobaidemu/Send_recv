CC = g++
SRCS = net.cpp test2.cpp
OPTS = -m64 -std=c++11 -w
OBJS = $(SRCS:.cpp=.o)
LIB = -lpthread
EXEC = test
#$< 表示所有依赖目标集 $@表示目标集 -o的意思表示输出为后面的参数
%.o : %.cpp
	$(CC) $(OPTS) -c $< -o $@
 
start:$(OBJS)
	$(CC) $(OPTS) $(OBJS) -o $(EXEC) $(LIB)

.PHONY : clean
clean:
	-rm -f test $(OBJS)
