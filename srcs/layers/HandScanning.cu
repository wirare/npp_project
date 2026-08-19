#include "camera_control.hpp"

#include <algorithm>
#include <cstdio>
#include <cuda_runtime.h>
#include <npp.h>

#include <filter.hpp>
#include <memory.hpp>
#include <cuda_global.hpp>
#include <global.hpp>


static __global__ void background_substraction_kernel(
	const Npp8u *current,
	Npp32s currentStep,
	const Npp8u *bg,
	Npp32s bgStep,
	Npp8u *mask,
	Npp32s maskStep,
	int width,
	int height,
	Npp8u threshold)
{
	const int x = blockIdx.x * blockDim.x + threadIdx.x;
	const int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x >= width || y >= height)
		return;

	const Npp8u *currentRow = current + static_cast<size_t>(y) * currentStep;
	const Npp8u *bgRow = bg + static_cast<size_t>(y) * bgStep;

	Npp8u *maskRow = mask + static_cast<size_t>(y) * maskStep;

	const Npp8u *currentPx = currentRow + static_cast<size_t>(x) * 4;
	const Npp8u *bgPx = bgRow + static_cast<size_t>(x) * 4;

	const int dr = abs(static_cast<int>(currentPx[0]) - static_cast<int>(bgPx[0]));
	const int dg = abs(static_cast<int>(currentPx[1]) - static_cast<int>(bgPx[1]));
	const int db = abs(static_cast<int>(currentPx[2]) - static_cast<int>(bgPx[2]));

	const int maxDiff = max(dr, max(dg, db));

	maskRow[x] = (maxDiff > threshold) ? 255 : 0;
}


static __global__ void reset_component_stats_kernel(
	HandComponentStats *stats,
	int count,
	int width,
	int height)
{
	const int i = blockIdx.x * blockDim.x + threadIdx.x;

	if (i >= count)
		return;

	stats[i].pixelCount = 0;

	stats[i].minX = width;
	stats[i].minY = height;

	stats[i].maxX = -1;
	stats[i].maxY = -1;
}


static __global__ void component_stats_kernel(
	const Npp8u *fgMask,
	Npp32s fgMaskStep,
	const Npp32u *labels,
	Npp32s labelsStep,
	HandComponentStats *stats,
	int maxLabelId,
	int width,
	int height)
{
	const int x = blockIdx.x * blockDim.x + threadIdx.x;
	const int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x >= width || y >= height)
		return;

	const Npp8u *fgRow = fgMask + static_cast<size_t>(y) * fgMaskStep;

	if (fgRow[x] != 255)
		return;

	const Npp32u *labelRow = reinterpret_cast<const Npp32u*>(
		reinterpret_cast<const Npp8u*>(labels) + static_cast<size_t>(y) * labelsStep
	);

	const Npp32u label = labelRow[x];

	if (label > static_cast<Npp32u>(maxLabelId))
		return;

	HandComponentStats *component = &stats[label];

	atomicAdd(&component->pixelCount, 1u);

	atomicMin(&component->minX, x);
	atomicMin(&component->minY, y);

	atomicMax(&component->maxX, x);
	atomicMax(&component->maxY, y);
}


static __global__ void extract_component_kernel(
	const Npp8u *fgMask,
	Npp32s fgMaskStep,
	const Npp32u *labels,
	Npp32s labelsStep,
	Npp8u *handMask,
	Npp32s handMaskStep,
	Npp32u selectedLabel,
	int width,
	int height)
{
	const int x = blockIdx.x * blockDim.x + threadIdx.x;
	const int y = blockIdx.y * blockDim.y + threadIdx.y;

	if (x >= width || y >= height)
		return;

	const Npp8u *fgRow = fgMask + static_cast<size_t>(y) * fgMaskStep;

	const Npp32u *labelRow = reinterpret_cast<const Npp32u*>(
		reinterpret_cast<const Npp8u*>(labels) + static_cast<size_t>(y) * labelsStep
	);

	Npp8u *handRow = handMask + static_cast<size_t>(y) * handMaskStep;

	handRow[x] = (
		fgRow[x] == 255 &&
		labelRow[x] == selectedLabel
	) ? 255 : 0;
}


std::vector<bufferRequest> HandScanning::getRequestedBuffers()
{
	return {
		REQUEST(BUF_8u_C4, STATUS_PRIVATE, 1),
		REQUEST(BUF_8u_C1, STATUS_PUBLIC, 2),
		REQUEST(BUF_8u_C4, STATUS_PUBLIC, 1),
	};
}


bool HandScanning::init()
{
	ASSIGN_BUFFER(BUF_8u_C4, MEM_PRIVATE, 0, bg, bgStep);
	ASSIGN_BUFFER(BUF_8u_C1, MEM_PUBLIC, 0, fgMask, fgMaskStep);
	ASSIGN_BUFFER(BUF_8u_C1, MEM_PUBLIC, 1, handMask, handMaskStep);
	ASSIGN_BUFFER(BUF_8u_C4, MEM_PUBLIC, 0, gaussian, gaussianStep);


	labelsStep = g_w * sizeof(Npp32u);

	cudaError_t cudaStatus = cudaMalloc(
		reinterpret_cast<void**>(&labels),
		static_cast<size_t>(labelsStep) * g_h
	);

	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "Failed to allocate label image: %s\n", cudaGetErrorString(cudaStatus));
		return false;
	}

	g_cuda_buf_to_free.push_back(labels);

	NppiSize roi = {g_w, g_h};

	int labelBufferSize = 0;

	NppStatus status = nppiLabelMarkersUFGetBufferSize_32u_C1R(
		roi,
		&labelBufferSize
	);

	if (status != NPP_SUCCESS)
	{
		fprintf(stderr, "nppiLabelMarkersUFGetBufferSize failed: %d\n", status);
		return false;
	}

	labelStartingNumber = g_w * g_h;

	int compressBufferSize = 0;

	status = nppiCompressMarkerLabelsGetBufferSize_32u_C1R(
		labelStartingNumber,
		&compressBufferSize
	);

	if (status != NPP_SUCCESS)
	{
		fprintf(stderr, "CompressMarkerLabelsGetBufferSize failed: %d\n", status);
		return false;
	}

	labelWorkBufferSize = std::max(
		labelBufferSize,
		compressBufferSize
	);

	cudaStatus = cudaMalloc(
		reinterpret_cast<void**>(&labelWorkBuffer),
		labelWorkBufferSize
	);

	if (cudaStatus != cudaSuccess)
	{
		fprintf(stderr, "Failed to allocate label workspace: %s\n", cudaGetErrorString(cudaStatus));
		return false;
	}

	g_cuda_buf_to_free.push_back(labelWorkBuffer);

	return true;
}


void HandScanning::activate()
{
}


bool HandScanning::handleBackgroundState(Npp8u *in, Npp32s inStep, NppiSize roi)
{
	if (g_set_bg)
	{
		bg_capture_countdown = 3;
		bg_valid = false;

		g_set_bg = false;

		setCameraAutoControlsLocked(true);
	}

	if (bg_capture_countdown > 0)
	{
		displayInput(in, inStep, roi);

		bg_capture_countdown--;

		return true;
	}
	else if (bg_capture_countdown == 0)
	{
		NppStatus status = nppiFilterGauss_8u_C4R_Ctx(
			in,
			inStep,
			gaussian,
			gaussianStep,
			roi,
			NPP_MASK_SIZE_3_X_3,
			g_nppStreamCtx
		);

		if (status != NPP_SUCCESS)
		{
			fprintf(stderr, "Background gaussian failed: %d\n", status);

			bg_capture_countdown = -1;

			return true;
		}

		status = nppiCopy_8u_C4R_Ctx(
			gaussian,
			gaussianStep,
			bg,
			bgStep,
			roi,
			g_nppStreamCtx
		);

		if (status != NPP_SUCCESS)
		{
			fprintf(stderr, "Background copy failed: %d\n", status);

			bg_capture_countdown = -1;

			return true;
		}

		displayInput(in, inStep, roi);

		bg_capture_countdown = -1;
		bg_valid = true;

		return true;
	}

	if (bg_valid == false)
	{
		displayInput(in, inStep, roi);
		return true;
	}

	return false;
}


bool HandScanning::generateForegroundMask(Npp8u *in, Npp32s inStep)
{
	NppiSize roi = {g_w, g_h};

	NppStatus status = nppiFilterGauss_8u_C4R_Ctx(
		in,
		inStep,
		gaussian,
		gaussianStep,
		roi,
		NPP_MASK_SIZE_3_X_3,
		g_nppStreamCtx
	);

	if (status != NPP_SUCCESS)
	{
		fprintf(stderr, "Gaussian failed: %d\n", status);
		return false;
	}

	dim3 block(32, 8);

	dim3 grid(
		(g_w + block.x - 1) / block.x,
		(g_h + block.y - 1) / block.y
	);

	background_substraction_kernel<<<grid, block, 0, g_stream>>>(
		gaussian,
		gaussianStep,
		bg,
		bgStep,
		fgMask,
		fgMaskStep,
		g_w,
		g_h,
		30
	);

	cudaError_t err = cudaPeekAtLastError();

	if (err != cudaSuccess)
	{
		fprintf(stderr, "background_subtraction_kernel launch failed: %s\n", cudaGetErrorString(err));
		return false;
	}

	return true;
}

bool HandScanning::closeForegroundMask(Npp8u *in, Npp32s inStep)
{

}

bool HandScanning::labelComponents(NppiSize roi, int &maxLabelId)
{
	NppStatus status = nppiLabelMarkersUF_8u32u_C1R_Ctx(
		fgMask,
		fgMaskStep,
		labels,
		labelsStep,
		roi,
		nppiNormInf,
		labelWorkBuffer,
		g_nppStreamCtx
	);

	if (status != NPP_SUCCESS)
	{
		fprintf(stderr, "LabelMarkersUF failed: %d\n", status);
		return false;
	}

	maxLabelId = 0;

	status = nppiCompressMarkerLabelsUF_32u_C1IR_Ctx(
		labels,
		labelsStep,
		roi,
		labelStartingNumber,
		&maxLabelId,
		labelWorkBuffer,
		g_nppStreamCtx
	);

	if (status != NPP_SUCCESS)
	{
		fprintf(stderr, "CompressMarkerLabelsUF failed: %d\n", status);
		return false;
	}

	cudaError_t err = cudaStreamSynchronize(g_stream);

	if (err != cudaSuccess)
	{
		fprintf(stderr, "CUDA sync after compression failed: %s\n", cudaGetErrorString(err));
		return false;
	}

	return true;
}


bool HandScanning::ensureComponentStatsCapacity(size_t requiredStatsCount)
{
	if (requiredStatsCount <= componentStatsCapacity)
		return true;

	size_t newCapacity = 1;

	while (newCapacity < requiredStatsCount)
		newCapacity *= 2;


	if (componentStatsDev != nullptr)
	{
		cudaFree(componentStatsDev);
		componentStatsDev = nullptr;
	}

	cudaError_t err = cudaMalloc(
		reinterpret_cast<void**>(&componentStatsDev),
		newCapacity * sizeof(HandComponentStats)
	);

	if (err != cudaSuccess)
	{
		fprintf(stderr, "cudaMalloc componentStatsDev failed: %s\n", cudaGetErrorString(err));
		return false;
	}

	componentStatsCapacity = newCapacity;

	return true;
}


bool HandScanning::updateComponentStats(int maxLabelId)
{
	const size_t requiredStatsCount = static_cast<size_t>(maxLabelId) + 1;

	if (ensureComponentStatsCapacity(requiredStatsCount) == false)
		return false;


	const int statsBlockSize = 256;

	const int statsGridSize = (
		static_cast<int>(requiredStatsCount) +
		statsBlockSize - 1
	) / statsBlockSize;


	reset_component_stats_kernel<<<
		statsGridSize,
		statsBlockSize,
		0,
		g_stream>>>(
			componentStatsDev,
			static_cast<int>(requiredStatsCount),
			g_w,
			g_h
		);

	cudaError_t err = cudaPeekAtLastError();

	if (err != cudaSuccess)
	{
		fprintf(stderr, "reset_component_stats_kernel launch failed: %s\n", cudaGetErrorString(err));
		return false;
	}


	dim3 block(32, 8);

	dim3 grid(
		(g_w + block.x - 1) / block.x,
		(g_h + block.y - 1) / block.y
	);

	component_stats_kernel<<<grid, block, 0, g_stream>>>(
		fgMask,
		fgMaskStep,
		labels,
		labelsStep,
		componentStatsDev,
		maxLabelId,
		g_w,
		g_h
	);

	err = cudaPeekAtLastError();

	if (err != cudaSuccess)
	{
		fprintf(stderr, "component_stats_kernel launch failed: %s\n", cudaGetErrorString(err));
		return false;
	}

	err = cudaStreamSynchronize(g_stream);

	if (err != cudaSuccess)
	{
		fprintf(stderr, "CUDA sync after component stats failed: %s\n", cudaGetErrorString(err));
		return false;
	}

	componentStatsHost.resize(requiredStatsCount);

	err = cudaMemcpy(
		componentStatsHost.data(),
		componentStatsDev,
		requiredStatsCount * sizeof(HandComponentStats),
		cudaMemcpyDeviceToHost
	);

	if (err != cudaSuccess)
	{
		fprintf(stderr, "cudaMemcpy component stats failed: %s\n", cudaGetErrorString(err));
		return false;
	}

	return true;
}


Npp32u HandScanning::findHandLabel(unsigned int &bestArea)
{
	Npp32u bestLabel = 0xFFFFFFFFu;

	bestArea = 0;

	const unsigned int minHandArea = static_cast<unsigned int>(
		static_cast<size_t>(g_w) *
		static_cast<size_t>(g_h) *
		0.005
	);

	for (size_t label = 0; label < componentStatsHost.size(); label++)
	{
		const HandComponentStats &stats = componentStatsHost[label];

		if (stats.pixelCount < minHandArea)
			continue;

		if (stats.pixelCount > bestArea)
		{
			bestArea = stats.pixelCount;
			bestLabel = static_cast<Npp32u>(label);
		}
	}

	return bestLabel;
}


bool HandScanning::extractHandMask(Npp32u label)
{
	dim3 block(32, 8);

	dim3 grid(
		(g_w + block.x - 1) / block.x,
		(g_h + block.y - 1) / block.y
	);

	extract_component_kernel<<<grid, block, 0, g_stream>>>(
		fgMask,
		fgMaskStep,
		labels,
		labelsStep,
		handMask,
		handMaskStep,
		label,
		g_w,
		g_h
	);

	cudaError_t err = cudaPeekAtLastError();

	if (err != cudaSuccess)
	{
		fprintf(stderr, "extract_component_kernel launch failed: %s\n", cudaGetErrorString(err));
		return false;
	}

	return true;
}


void HandScanning::debugHand(Npp32u label, unsigned int area)
{
	if (debugTimer > 0)
	{
		debugTimer--;
		return;
	}

	if (area == 0 || label == 0xFFFFFFFFu)
	{
		printf("No foreground component\n");
		debugTimer = 100;
		return;
	}


	const HandComponentStats &stats = componentStatsHost[label];

	const int bboxWidth = stats.maxX - stats.minX + 1;
	const int bboxHeight = stats.maxY - stats.minY + 1;


	printf(
		"Hand candidate: "
		"label=%u "
		"area=%u "
		"bbox=(%d,%d %dx%d)\n",
		label,
		stats.pixelCount,
		stats.minX,
		stats.minY,
		bboxWidth,
		bboxHeight
	);


	debugTimer = 100;
}


void HandScanning::displayInput(Npp8u *in, Npp32s inStep, NppiSize roi)
{
	nppiCopy_8u_C4R_Ctx(in, inStep, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
}


void HandScanning::displayMask(Npp8u *mask, Npp32s maskStep, NppiSize roi)
{
	nppiCopy_8u_C1C4R_Ctx(mask, maskStep, g_rgba_out, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(mask, maskStep, g_rgba_out + 1, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiCopy_8u_C1C4R_Ctx(mask, maskStep, g_rgba_out + 2, g_rgba_outStep, roi, g_nppStreamCtx);
	nppiSet_8u_C4CR_Ctx(255, g_rgba_out + 3, g_rgba_outStep, roi, g_nppStreamCtx);
}


void HandScanning::process(Npp8u *in, Npp32s inStep)
{
	NppiSize roi = {g_w, g_h};


	if (handleBackgroundState(in, inStep, roi))
		return;


	if (generateForegroundMask(in, inStep) == false)
		return;


	int maxLabelId = 0;

	if (labelComponents(roi, maxLabelId) == false)
		return;


	if (updateComponentStats(maxLabelId) == false)
		return;


	unsigned int bestArea = 0;

	Npp32u bestLabel = findHandLabel(bestArea);


	debugHand(bestLabel, bestArea);


	if (extractHandMask(bestLabel) == false)
		return;


	displayMask(
		fgMask,
		fgMaskStep,
		roi
	);
}


void HandScanning::deactivate()
{
	setCameraAutoControlsLocked(false);

	bg_valid = false;
	bg_capture_countdown = -1;
}


std::string HandScanning::getName()
{
	return "HandScanning";
}


HandScanning::~HandScanning()
{
	if (componentStatsDev != nullptr)
	{
		cudaFree(componentStatsDev);
		componentStatsDev = nullptr;
	}
}
