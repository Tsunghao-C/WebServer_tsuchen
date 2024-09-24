# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: okoca <okoca@student.42.fr>                +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2024/09/23 14:04:06 by bthomas           #+#    #+#              #
#    Updated: 2024/09/24 21:43 by okoca            ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME		=	webserv
SRCDIR		=	src
OBJDIR		=	obj
CFLAGS		=	-Wall -Werror -Wextra -std=c++98 -MMD
CPP			=	c++

INCS		=	-I ./include
INCS		+=	-I ./include/parser

SRC			=	main.cpp
SRC			+=	parser.cpp lexer.cpp

vpath %.cpp src/
vpath %.cpp src/parser

OBJS		= $(addprefix $(OBJDIR)/, $(SRC:.cpp=.o))

DEPS = $(OBJS:%.o=%.d)

all: $(NAME)

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CPP) $(CFLAGS) $(INCS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(NAME): $(OBJDIR) $(OBJS)
	$(CPP) $(CFLAGS) $(INCS) -o $(NAME) $(OBJS)
	@echo "webserv build done!"

-include $(DEPS)

clean:
	@echo "Cleaning object files..."
	rm -rf $(OBJDIR)

fclean: clean
	@echo "Removing webserv..."
	rm -f $(NAME)

re: fclean
	make all

.PHONY: all clean fclean re
