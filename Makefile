# NAME = irc <- commented this out becuz subject pdf wants prog name to be ircserv!
NAME = ircserv

SRCS = main.cpp Server.cpp Client.cpp Commands.cpp Utils.cpp Channel.cpp

CXXFLAGS = -Wall -Wextra -Werror 
# -std=c++98

CXX = c++

OBJS = $(SRCS:.cpp=.o)

$(NAME): $(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS) -o $(NAME)

all: $(NAME)

clean:
	$(RM) $(OBJS)
#	rm -rf *.o     <- this isn't safe! better alt is rm -f $(OBJS)

fclean: clean
	rm -rf $(NAME)

re: fclean all

.PHONY: all clean fclean re
