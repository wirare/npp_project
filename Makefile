NAME        := npp

SRC_DIR     := srcs
INC_ROOT    := includes
BUILD_DIR   := .build

SYSROOT     ?= $(SDKTARGETSYSROOT)
OECORE_NATIVE_SYSROOT ?= $(OECORE_NATIVE_SYSROOT)

CROSS_COMPILE ?= aarch64-poky-linux-
TARGET_TRIPLET := $(patsubst %-,%,$(CROSS_COMPILE))

CC          ?= $(CROSS_COMPILE)gcc
CXX         ?= $(CROSS_COMPILE)g++
PKG_CONFIG  ?= pkg-config


CUDA_HOST_CXX ?= $(firstword \
	$(wildcard $(OECORE_NATIVE_SYSROOT)/usr/bin/$(TARGET_TRIPLET)/$(TARGET_TRIPLET)-g++-8.5.0) \
	$(wildcard $(OECORE_NATIVE_SYSROOT)/usr/bin/$(TARGET_TRIPLET)/$(TARGET_TRIPLET)-g++))

CUDA_TARGET_HOME ?= $(firstword $(wildcard $(SYSROOT)/usr/local/cuda $(SYSROOT)/usr/local/cuda-*))
CUDA_HOST_HOME   ?= /usr/local/cuda-11.4
NVCC             ?= $(CUDA_HOST_HOME)/bin/nvcc

ifeq ($(strip $(SYSROOT)),)
$(error SDKTARGETSYSROOT is empty; source the Yocto SDK environment script first)
endif

ifeq ($(strip $(OECORE_NATIVE_SYSROOT)),)
$(error OECORE_NATIVE_SYSROOT is empty; source the Yocto SDK environment script first)
endif

ifeq ($(strip $(CUDA_TARGET_HOME)),)
$(error Target CUDA installation not found under $(SYSROOT)/usr/local)
endif

ifeq ($(wildcard $(NVCC)),)
$(error Host nvcc not found: $(NVCC))
endif

ifeq ($(strip $(CUDA_HOST_CXX)),)
$(error CUDA host C++ compiler not found)
endif

CUDA_INC    := -I$(CUDA_TARGET_HOME)/include
CUDA_LIBDIR := -L$(CUDA_TARGET_HOME)/lib64 -L$(CUDA_TARGET_HOME)/lib

JETSON_MM_INC := -I$(SYSROOT)/usr/src/jetson_multimedia_api/include
JETSON_MM_LIB := -L$(SYSROOT)/usr/lib/aarch64-linux-gnu/nvidia \
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

PKG_INCFLAGS   := $(filter -I% -D%,$(GST_CFLAGS) $(GTK_CFLAGS))
PKG_HOSTFLAGS  := $(filter-out -I% -D%,$(GST_CFLAGS) $(GTK_CFLAGS))

NVCC_CPPFLAGS := $(INCFLAGS) $(CUDA_INC) $(JETSON_MM_INC) $(PKG_INCFLAGS)

NVCCFLAGS += -O3 -std=c++17 \
             -ccbin $(CUDA_HOST_CXX) \
             -Xcompiler -fPIC \
             -Xcompiler --sysroot=$(SYSROOT) \
             --generate-code arch=compute_87,code=sm_87 \
             $(NVCC_CPPFLAGS) \
			 -allow-unsupported-compiler

LDLIBS += \
	$(CUDA_LIBDIR) \
	$(JETSON_MM_LIB) \
	-lcuda -lcudart -lcudadevrt \
	-lnppc -lnppif -lnppisu -lnppicc -lnppial -lnppidei -lnppig -lnppist \
	-lEGL \
	$(GST_LIBS) \
	-ldl -lpthread -lm \
	-lnvbufsurface \
	$(GTK_LIBS)

SRCS_C      := $(shell find $(SRC_DIR) -type f -name "*.c")
SRCS_CPP    := $(shell find $(SRC_DIR) -type f -name "*.cpp")
SRCS_CU     := $(shell find $(SRC_DIR) -type f -name "*.cu")
SRCS        := $(SRCS_C) $(SRCS_CPP) $(SRCS_CU)

OBJS        := $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(SRCS))
OBJS        := $(OBJS:.c=.o)
OBJS        := $(OBJS:.cpp=.o)
OBJS        := $(OBJS:.cu=.o)

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

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cu
	@mkdir -p $(dir $@)
	$(NVCC) $(NVCCFLAGS) -c $< -o $@

clean:
	$(RM) -r $(BUILD_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

print-toolchain:
	@echo "SYSROOT=$(SYSROOT)"
	@echo "OECORE_NATIVE_SYSROOT=$(OECORE_NATIVE_SYSROOT)"
	@echo "CC=$(CC)"
	@echo "CXX=$(CXX)"
	@echo "NVCC=$(NVCC)"
	@echo "CUDA_HOST_CC=$(CUDA_HOST_CC)"
	@echo "CUDA_HOST_CXX=$(CUDA_HOST_CXX)"
	@echo "CUDA_HOME=$(CUDA_HOME)"

-include $(DEPS)

.PHONY: all clean fclean re print-toolchain
