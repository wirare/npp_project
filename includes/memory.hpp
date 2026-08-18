#pragma once

#include <array>
#include <iostream>
#include <string>
#include <vector>

#include <npp.h>

typedef enum
{
	BUF_8u_C1,
	BUF_8u_C4,
	BUF_16s_C1,
	BUF_16s_C4,
	BUF_32f_C1,
	BUF_END,
} bufferType_t;

typedef void* (*buff_malloc_fn)(int, int, Npp32s*);

template<auto Fn>
static void* malloc_wrap(int w, int h, Npp32s* step)
{
	return static_cast<void*>(Fn(w, h, step));
}

#define BUF_TO_MALLOC(t) &malloc_wrap<nppiMalloc_##t>

const std::array<buff_malloc_fn, BUF_END + 1> bufTypeToMallocMapping =
{
	BUF_TO_MALLOC(8u_C1),
	BUF_TO_MALLOC(8u_C4),
	BUF_TO_MALLOC(16s_C1),
	BUF_TO_MALLOC(16s_C4),
	BUF_TO_MALLOC(32f_C1),
	nullptr,
};

typedef enum
{
	MEM_PUBLIC,
	MEM_PRIVATE,
} memoryVisibility_t;

typedef enum
{
	STATUS_PUBLIC,
	STATUS_PRIVATE
} bufferStatus_t;

typedef struct
{
	int nbOfBuffer;
	bufferType_t bufferType;
	bufferStatus_t bufferStatus;
} bufferRequest;

typedef struct
{
	bufferType_t type;
	bufferStatus_t status;
	std::vector<bool> taken;
	std::vector<void*> buffers;
	std::vector<Npp32s> steps;
} buffers_t;

#define CASE_T_TO_STR(t) case t: return std::string(#t)

static std::string bufferTypeToString(bufferType_t t)
{
	switch (t)
	{
		CASE_T_TO_STR(BUF_8u_C1);
		CASE_T_TO_STR(BUF_8u_C4);
		CASE_T_TO_STR(BUF_16s_C1);
		CASE_T_TO_STR(BUF_16s_C4);
		CASE_T_TO_STR(BUF_32f_C1);
		case BUF_END:
			std::cerr << "BUF_END Should never be used for a buffer\n";
			return std::string("BUF_END");
	}
	__builtin_unreachable();
}

static std::string memoryVisibilityToString(memoryVisibility_t t)
{
	switch (t)
	{
		case MEM_PUBLIC: return std::string("MEM_PUBLIC");
		case MEM_PRIVATE: return std::string("MEM_PRIVATE");
	}
	__builtin_unreachable();
}

#define REQUEST(t, s, n) bufferRequest{n, t, s}

std::vector<bufferRequest> mergeBufferRequests(const std::vector<std::vector<bufferRequest>>& requestSets);
bool initBuffers(const std::vector<bufferRequest>& requests);
int getBuffer(bufferType_t bufferType, memoryVisibility_t visibility, int buf_idx, void **dest, Npp32s *step);
void resize_buffer(void **buffer, Npp32s *step, int w, int h);
void freeBuffers();
