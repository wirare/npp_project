NAME        := npp

SRC_DIR     := srcs
INC_ROOT    := includes
BUILD_DIR   := .build

CC          ?= gcc
CXX         ?= g++
PKG_CONFIG  ?= pkg-config

SYSROOT     ?= $(SDKTARGETSYSROOT)

CUDA_ROOT_CANDIDATES := \
	$(SYSROOT)/usr/local/cuda \
	$(firstword $(sort $(wildcard $(SYSROOT)/usr/local/cuda-*)))

CUDA_HOME   ?= $(firstword $(wildcard $(SYSROOT)/usr/local/cuda $(SYSROOT)/usr/local/cuda-*))

ifeq ($(strip $(CUDA_HOME)),)
$(error CUDA installation not found under $(SYSROOT)/usr/local)
endif

CUDA_INC    := -I$(CUDA_HOME)/include
CUDA_LIBDIR := -L$(CUDA_HOME)/lib

JETSON_MM_INC   := -I$(SYSROOT)/usr/src/jetson_multimedia_api/include
JETSON_MM_LIB   := -L$(SYSROOT)/usr/lib/aarch64-linux-gnu/nvidia \
                   -L$(SYSROOT)/usr/lib/aarch64-linux-gnu

GST_CFLAGS  := $(shell $(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-app-1.0 gstreamer-allocators-1.0)
GST_LIBS    := $(shell $(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-app-1.0 gstreamer-allocators-1.0)

GTK_CFLAGS  := $(shell $(PKG_CONFIG) --cflags gtk+-3.0)
GTK_LIBS    := $(shell $(PKG_CONFIG) --libs gtk+-3.0)

INC_DIRS    := $(shell find $(INC_ROOT) -type d 2>/dev/null)
INCFLAGS    := $(addprefix -I,$(INC_DIRS))

COMMON_INC  := $(INCFLAGS) $(CUDA_INC) $(JETSON_MM_INC) $(GST_CFLAGS) $(GTK_CFLAGS)

CFLAGS      += -Wall -Wextra -Werror -O3 -std=c11   -MMD -MP $(COMMON_INC) 
CXXFLAGS    += -Wall -Wextra -Werror -O3 -std=c++17 -MMD -MP $(COMMON_INC) -Wno-unused-function 
LDLIBS      += \
	$(CUDA_LIBDIR) \
	$(JETSON_MM_LIB) \
	-lcuda -lcudart \
	-lnppc -lnppif -lnppisu -lnppicc -lnppial -lnppidei -lnppig\
	-lEGL \
	$(GST_LIBS) \
	-ldl -lpthread -lm \
	-lnvbufsurface \
	$(GTK_LIBS)

SRCS_C      := $(shell find $(SRC_DIR) -type f -name "*.c")
SRCS_CPP    := $(shell find $(SRC_DIR) -type f -name "*.cpp")
SRCS        := $(SRCS_C) $(SRCS_CPP)

OBJS        := $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(SRCS))
OBJS        := $(OBJS:.c=.o)
OBJS        := $(OBJS:.cpp=.o)

DEPS        := $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) -r $(BUILD_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re
