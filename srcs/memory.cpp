#include <npp.h>
#include <nppi.h>
#include <memory.hpp>
#include <global.hpp>
#include <iostream>

#define CASE_INDEX(t, i) case(MEM_##t): index = i; break
static std::vector<buffers_t> g_buffers;

void resize_buffer(void **buffer, Npp32s *step, int w, int h)
{
	for (auto& buffers : g_buffers)
	{
		for (size_t i = 0; i != buffers.buffers.size(); i++)
		{
			void *buf = buffers.buffers[i];

			if (*buffer == buf)
			{
				auto malloc_fn = bufTypeToMallocMapping[buffers.type];
				nppiFree(buf);
				*buffer = malloc_fn(w, h, step);
				buffers.steps[i] = *step;
				buffers.buffers[i] = *buffer;
				return;
			}
		}
	}
	return;
}

void freeBuffers()
{
	for (auto buffer : g_buffers)
	{
		for (auto buff : buffer.buffers)
			nppiFree(buff);
	}
}

int initBuffers()
{
	static bool init = false;

	if (!init)
	{
		for (auto req : bufferAtInit)
		{
			buffers_t current;

			current.type = req.bufferType;
			current.status = req.bufferStatus;
			current.taken = false;
			current.buffers.resize(req.nbOfBuffer);
			current.steps.resize(req.nbOfBuffer);

			if (req.bufferType == BUF_END)
				return 1;

			for (int i = 0; i != req.nbOfBuffer; i++)
			{
				auto malloc_fn = bufTypeToMallocMapping[req.bufferType];
				current.buffers[i] = malloc_fn(g_w, g_h, &current.steps[i]);
			}
			g_buffers.push_back(current);
		}
		init = true;
	}
	return 0;
}

int getBuffer(bufferType_t bufferType, memoryVisibility_t visibility, int buf_idx, void **dest, Npp32s *step)
{
	if (visibility == MEM_PRIVATE)
	{
		for (auto& buffer : g_buffers)
		{
			if (buffer.status == STATUS_PRIVATE && buffer.type == bufferType && !buffer.taken)
			{
				*dest = buffer.buffers[0];
				*step = buffer.steps[0];
				buffer.taken = true;
				return 0;
			}
		}
		std::cerr << "Couldnt manage to get buffer for MEM_PRIVATE\n";
		return 1;
	}

	for (auto buffer : g_buffers)
	{
		if (buffer.type == bufferType && buffer.status == STATUS_PUBLIC)
		{
			if (buf_idx < 0 && buf_idx >= (int)buffer.buffers.size())
			{
				std::cerr << "Buffer index retreiving is wrong\n";
				return 1;
			}

			*dest = buffer.buffers[buf_idx];
			*step = buffer.steps[buf_idx];
			return 0;
		}
	}
	std::cerr << "Couldnt manage to get buffer for MEM_PUBLIC\n";
	return 1;
}
