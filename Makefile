NAME        := npp

SRC_DIR     := srcs
INC_ROOT    := includes
BUILD_DIR   := .build

SYSROOT        := $(strip $(SDKTARGETSYSROOT))
NATIVE_SYSROOT := $(strip $(OECORE_NATIVE_SYSROOT))

PKG_CONFIG ?= pkg-config

ifeq ($(SYSROOT),)
$(error SDKTARGETSYSROOT is empty; source the Yocto SDK environment script first)
endif

ifeq ($(NATIVE_SYSROOT),)
$(error OECORE_NATIVE_SYSROOT is empty; source the Yocto SDK environment script first)
endif

ifeq ($(strip $(CUDACXX)),)
$(error CUDACXX is empty; the SDK is missing nativesdk-cuda-environment)
endif

ifeq ($(strip $(CUDAHOSTCXX)),)
$(error CUDAHOSTCXX is empty; the SDK is missing cuda-target-environment)
endif

ifeq ($(strip $(CUDA_PATH)),)
$(error CUDA_PATH is empty; the SDK CUDA target environment is incomplete)
endif

ifeq ($(strip $(CUDAFLAGS)),)
$(error CUDAFLAGS is empty; the SDK CUDA environment was not loaded)
endif

NVCC             := $(CUDACXX)
CUDA_HOST_CXX    := $(CUDAHOSTCXX)
CUDA_TARGET_HOME := $(CUDA_PATH)

MMAPI_HEADER := $(firstword \
	$(wildcard $(SYSROOT)/usr/include/nvbufsurface.h) \
	$(wildcard $(SYSROOT)/usr/src/jetson_multimedia_api/include/nvbufsurface.h))

ifeq ($(strip $(MMAPI_HEADER)),)
$(error nvbufsurface.h was not found; add tegra-mmapi-dev to TOOLCHAIN_TARGET_TASK and rebuild the SDK)
endif

MMAPI_INCLUDE_DIR := $(patsubst %/,%,$(dir $(MMAPI_HEADER)))
JETSON_MM_INC     := -I$(MMAPI_INCLUDE_DIR)

GST_CFLAGS := $(shell $(PKG_CONFIG) --cflags \
	gstreamer-1.0 \
	gstreamer-app-1.0 \
	gstreamer-allocators-1.0)

GST_LIBS := $(shell $(PKG_CONFIG) --libs \
	gstreamer-1.0 \
	gstreamer-app-1.0 \
	gstreamer-allocators-1.0)

GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk+-3.0)
GTK_LIBS   := $(shell $(PKG_CONFIG) --libs gtk+-3.0)

INC_DIRS := $(shell find $(INC_ROOT) -type d 2>/dev/null)
INCFLAGS := $(addprefix -I,$(INC_DIRS))

COMMON_INC := \
	$(INCFLAGS) \
	$(CUDA_CXXFLAGS) \
	$(JETSON_MM_INC) \
	$(GST_CFLAGS) \
	$(GTK_CFLAGS)

CFLAGS += \
	-Wall \
	-Wextra \
	-Werror \
	-O3 \
	-std=c11 \
	-MMD \
	-MP \
	$(COMMON_INC)

CXXFLAGS += \
	-Wall \
	-Wextra \
	-Werror \
	-O3 \
	-std=c++17 \
	-MMD \
	-MP \
	-Wno-unused-function \
	$(COMMON_INC)

CUDA_PROJECT_INC := \
	$(INCFLAGS) \
	$(JETSON_MM_INC)

CUDA_COMPILE_FLAGS := \
	-O3 \
	-std=c++17 \
	-ccbin $(CUDA_HOST_CXX) \
	$(CUDAFLAGS) \
	$(CUDA_PROJECT_INC) \
	-Xcompiler=-fPIC

LDLIBS += \
	$(CUDA_LDFLAGS) \
	-lcuda \
	-lcudart \
	-lcudadevrt \
	-lnppc \
	-lnppif \
	-lnppisu \
	-lnppicc \
	-lnppial \
	-lnppidei \
	-lnppig \
	-lnppist \
	-lEGL \
	$(GST_LIBS) \
	-ldl \
	-lpthread \
	-lm \
	-lnvbufsurface \
	$(GTK_LIBS)

SRCS_C   := $(shell find $(SRC_DIR) -type f -name '*.c')
SRCS_CPP := $(shell find $(SRC_DIR) -type f -name '*.cpp')
SRCS_CU  := $(shell find $(SRC_DIR) -type f -name '*.cu')
SRCS     := $(SRCS_C) $(SRCS_CPP) $(SRCS_CU)

OBJS := $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(SRCS))
OBJS := $(OBJS:.c=.o)
OBJS := $(OBJS:.cpp=.o)
OBJS := $(OBJS:.cu=.o)

DEPS := $(OBJS:.o=.d)

all: check-sdk $(NAME)

check-sdk:
	@command -v "$(NVCC)" >/dev/null || { \
		echo "CUDA compiler not found: $(NVCC)"; \
		false; \
	}
	@command -v "$(CUDA_HOST_CXX)" >/dev/null || { \
		echo "CUDA host compiler not found: $(CUDA_HOST_CXX)"; \
		false; \
	}
	@$(PKG_CONFIG) --exists \
		gstreamer-1.0 \
		gstreamer-app-1.0 \
		gstreamer-allocators-1.0 \
		gtk+-3.0 || { \
		echo "Required GStreamer or GTK pkg-config packages are missing"; \
		false; \
	}

$(NAME): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cu
	@mkdir -p $(dir $@)
	$(NVCC) $(CUDA_COMPILE_FLAGS) -c $< -o $@

clean:
	$(RM) -r $(BUILD_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

print-toolchain:
	@echo "SYSROOT=$(SYSROOT)"
	@echo "NATIVE_SYSROOT=$(NATIVE_SYSROOT)"
	@echo "CC=$(CC)"
	@echo "CXX=$(CXX)"
	@echo "NVCC=$(NVCC)"
	@echo "CUDA_HOST_CXX=$(CUDA_HOST_CXX)"
	@echo "CUDA_VERSION=$(CUDA_VERSION)"
	@echo "CUDA_PATH=$(CUDA_PATH)"
	@echo "CUDA_TOOLKIT_ROOT=$(CUDA_TOOLKIT_ROOT)"
	@echo "CUDA_NVCC_ARCH_FLAGS=$(CUDA_NVCC_ARCH_FLAGS)"
	@echo "MMAPI_INCLUDE_DIR=$(MMAPI_INCLUDE_DIR)"

-include $(DEPS)

.PHONY: all check-sdk clean fclean re print-toolchain
