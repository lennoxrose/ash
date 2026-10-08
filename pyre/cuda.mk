# CUDA backend detection, shared by pyre/Makefile and ashvm/Makefile so the
# "is CUDA here" decision and its link flags live in exactly one place.
#
# The CUDA backend is NVIDIA-only and optional: with no nvcc on this machine
# every variable below is empty and Pyre builds exactly as before (Vulkan +
# CPU). Override NVCC= or CUDA_ARCH= on the make command line if needed.
#
# CUDA_ARCH defaults to `native` (the GPU in this machine, so the kernels are
# compiled for exactly the hardware they will run on). Pass something like
# CUDA_ARCH='-gencode arch=compute_86,code=sm_86 -gencode arch=compute_89,code=sm_89'
# when building on one machine for another.
NVCC ?= $(firstword $(wildcard /opt/cuda/bin/nvcc /usr/local/cuda/bin/nvcc) $(shell command -v nvcc 2>/dev/null))
ifneq ($(NVCC),)
CUDA_HOME := $(abspath $(dir $(NVCC))/..)
CUDA_ARCH ?= -arch=native
# Static cudart: libpyre.so / ashvm then need only the NVIDIA driver's
# libcuda at run time, not a CUDA toolkit install.
CUDA_LIBS = -L$(CUDA_HOME)/lib64 -lcudart_static -lrt -lpthread
CUDA_DEFINE = -DASH_GPU_HAVE_CUDA
else
CUDA_LIBS =
CUDA_DEFINE =
endif
