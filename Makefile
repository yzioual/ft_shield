NAME := ft_shield

ifneq ($(filter all re $(NAME),$(MAKECMDGOALS)),)
ifeq ($(PASSWORD),)
		$(error Usage: make PASSWORD=your_password)
endif
endif

SRC_DIR := src
OBJ_DIR := obj

SRCS := src/ft_shield.c
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

HASH := $(shell printf '%s' '$(PASSWORD)' | md5sum | cut -d' ' -f1)
CC := gcc
CFLAGS := -Wall -Wextra -Werror -I$(SRC_DIR) -g
RM := rm -f

all: $(NAME)

$(NAME): $(OBJS)
	@echo "[ft_shield] Compiling $<..."
	$(CC) $(OBJS) -o $(NAME) -lm
	@echo "[ft_shield] $(NAME) build complete!"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	@$(CC) $(CFLAGS) -DPASSWORD='"$(HASH)"' -c $< -o $@

clean:
	@echo "[ft_shield] Cleaning object files..."
	@rm -rf $(OBJ_DIR)

fclean: clean
	@echo "[ft_shield] Removing $(NAME)..."
	@$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
