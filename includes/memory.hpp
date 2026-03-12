#pragma once

#include <array>
#include <utility>
#include <vector>
#include <npp.h>

typedef enum
{
	BUF_8u_C1,
	BUF_8u_C4,
	BUF_16s_C1,
	BUF_16s_C4,
	BUF_END,
}	bufferType_t;

typedef void* (*buff_malloc_fn)(int, int, Npp32s*);

template<auto Fn>
static void* malloc_wrap(int w, int h, Npp32s* step) {
  return static_cast<void*>(Fn(w, h, step));
}

#define BUF_TO_MALLOC(t) &malloc_wrap<nppiMalloc_##t>

const std::array<buff_malloc_fn, BUF_END+1> bufTypeToMallocMapping =
{
	BUF_TO_MALLOC(8u_C1),
	BUF_TO_MALLOC(8u_C4),
	BUF_TO_MALLOC(16s_C1),
	BUF_TO_MALLOC(16s_C4),
	nullptr,
};

typedef enum
{
	MEM_PUBLIC,
	MEM_PRIVATE,
}	memoryVisibility_t;

typedef enum
{
	STATUS_PUBLIC,
	STATUS_PRIVATE
}	bufferStatus_t;

typedef struct
{
	const int nbOfBuffer;
	bufferType_t bufferType;
	bufferStatus_t bufferStatus;
}	bufferRequest;

typedef struct
{
	bufferType_t type;
	bufferStatus_t status;
	bool taken;
	std::vector<void *> buffers;
	std::vector<Npp32s> steps;
}	buffers_t;

#define REQUEST(t, s, n) (bufferRequest){n, t, s}

const std::vector<bufferRequest> bufferAtInit =	
{
	REQUEST(BUF_8u_C4, STATUS_PRIVATE, 1),
	REQUEST(BUF_8u_C1, STATUS_PUBLIC, 2),
	REQUEST(BUF_16s_C1, STATUS_PUBLIC, 2),
	REQUEST(BUF_16s_C4, STATUS_PUBLIC, 3),
};

int getBuffer(bufferType_t bufferType, memoryVisibility_t visibility, int buf_idx, void **dest, Npp32s *step);
int initBuffers();
void freeBuffers();
