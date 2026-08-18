#include <memory.hpp>

#include <algorithm>
#include <array>
#include <iostream>
#include <utility>
#include <vector>

#include <global.hpp>
#include <npp.h>
#include <nppi.h>

static std::vector<buffers_t> g_buffers;

std::vector<bufferRequest> mergeBufferRequests(const std::vector<std::vector<bufferRequest>>& requestSets)
{
	std::array<int, BUF_END> publicCounts{};
	std::array<int, BUF_END> privateCounts{};

	for (const auto& requestSet : requestSets)
	{
		std::array<int, BUF_END> localPublicCounts{};
		std::array<int, BUF_END> localPrivateCounts{};

		for (const auto& request : requestSet)
		{
			if (request.bufferType < 0 || request.bufferType >= BUF_END || request.nbOfBuffer <= 0)
				continue;

			if (request.bufferStatus == STATUS_PUBLIC)
				localPublicCounts[request.bufferType] += request.nbOfBuffer;
			else
				localPrivateCounts[request.bufferType] += request.nbOfBuffer;
		}

		for (int type = 0; type < BUF_END; ++type)
		{
			publicCounts[type] = std::max(publicCounts[type], localPublicCounts[type]);
			privateCounts[type] += localPrivateCounts[type];
		}
	}

	std::vector<bufferRequest> merged;
	for (int type = 0; type < BUF_END; ++type)
	{
		if (privateCounts[type] > 0)
			merged.push_back(REQUEST(static_cast<bufferType_t>(type), STATUS_PRIVATE, privateCounts[type]));
		if (publicCounts[type] > 0)
			merged.push_back(REQUEST(static_cast<bufferType_t>(type), STATUS_PUBLIC, publicCounts[type]));
	}
	return merged;
}

void resize_buffer(void **buffer, Npp32s *step, int w, int h)
{
	for (auto& buffers : g_buffers)
	{
		for (size_t i = 0; i != buffers.buffers.size(); ++i)
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
}

void freeBuffers()
{
	for (auto& buffer : g_buffers)
	{
		for (auto buff : buffer.buffers)
		{
			if (buff)
				nppiFree(buff);
		}
	}
	g_buffers.clear();
}

bool initBuffers(const std::vector<bufferRequest>& requests)
{
	freeBuffers();

	for (const auto& req : requests)
	{
		if (req.bufferType < 0 || req.bufferType >= BUF_END || req.nbOfBuffer <= 0)
		{
			std::cerr << "Invalid buffer request\n";
			freeBuffers();
			return false;
		}

		buffers_t current;
		current.type = req.bufferType;
		current.status = req.bufferStatus;
		current.taken.resize(static_cast<size_t>(req.nbOfBuffer), false);
		current.buffers.resize(static_cast<size_t>(req.nbOfBuffer), nullptr);
		current.steps.resize(static_cast<size_t>(req.nbOfBuffer), 0);

		auto malloc_fn = bufTypeToMallocMapping[req.bufferType];
		if (!malloc_fn)
		{
			std::cerr << "No allocator for " << bufferTypeToString(req.bufferType) << "\n";
			freeBuffers();
			return false;
		}

		for (int i = 0; i < req.nbOfBuffer; ++i)
		{
			current.buffers[static_cast<size_t>(i)] = malloc_fn(g_w, g_h, &current.steps[static_cast<size_t>(i)]);
			if (!current.buffers[static_cast<size_t>(i)])
			{
				std::cerr << "Failed to allocate " << bufferTypeToString(req.bufferType) << "\n";
				for (auto buff : current.buffers)
				{
					if (buff)
						nppiFree(buff);
				}
				freeBuffers();
				return false;
			}
		}

		g_buffers.push_back(std::move(current));
	}
	return true;
}

static std::string getErrorStr(bufferType_t bufferType, memoryVisibility_t visibility, bool idx_err)
{
	std::string err("Error in buffer initialisation: ");
	if (idx_err)
		err += "Buffer index wrong for buffer of type " + bufferTypeToString(bufferType) + " with visibility MEM_PUBLIC\n";
	else if (visibility == MEM_PRIVATE)
		err += "No MEM_PRIVATE buffer of type " + bufferTypeToString(bufferType) + " available\n";
	else
		err += "No MEM_PUBLIC buffer of type " + bufferTypeToString(bufferType) + " exists\n";
	return err;
}

int getBuffer(bufferType_t bufferType, memoryVisibility_t visibility, int buf_idx, void **dest, Npp32s *step)
{
	if (!dest || !step)
		return 1;

	if (visibility == MEM_PRIVATE)
	{
		for (auto& buffer : g_buffers)
		{
			if (buffer.status != STATUS_PRIVATE || buffer.type != bufferType)
				continue;

			for (size_t i = 0; i != buffer.buffers.size(); ++i)
			{
				if (!buffer.taken[i])
				{
					*dest = buffer.buffers[i];
					*step = buffer.steps[i];
					buffer.taken[i] = true;
					return 0;
				}
			}
		}
		std::cerr << getErrorStr(bufferType, visibility, false);
		return 1;
	}

	for (auto& buffer : g_buffers)
	{
		if (buffer.type == bufferType && buffer.status == STATUS_PUBLIC)
		{
			if (buf_idx < 0 || buf_idx >= static_cast<int>(buffer.buffers.size()))
			{
				std::cerr << getErrorStr(bufferType, visibility, true);
				return 1;
			}

			*dest = buffer.buffers[static_cast<size_t>(buf_idx)];
			*step = buffer.steps[static_cast<size_t>(buf_idx)];
			return 0;
		}
	}
	std::cerr << getErrorStr(bufferType, visibility, false);
	return 1;
}
