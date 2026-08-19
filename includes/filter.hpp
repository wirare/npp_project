#pragma once

#include "nppdefs.h"
#include <memory>
#include <string>
#include <vector>

#include <npp.h>

#include <memory.hpp>

#define ASSIGN_BUFFER(type, vis, idx, buf, step)								\
	if (getBuffer(type, vis, idx, reinterpret_cast<void**>(&buf), &step) != 0)	\
		return false;

class AFilter
{
	public:
		virtual ~AFilter() = default;

		virtual std::vector<bufferRequest> getRequestedBuffers() = 0;
		virtual bool init() = 0;
		virtual void activate() = 0;
		virtual void process(Npp8u *in, Npp32s inStep) = 0;
		virtual void deactivate() = 0;
		virtual std::string getName() = 0;
};

class Empty : public AFilter
{
	public:
		std::vector<bufferRequest> getRequestedBuffers() override;
		bool init() override;
		void activate() override;
		void process(Npp8u *in, Npp32s inStep) override;
		void deactivate() override;
		std::string getName() override;
};

class SobelV : public AFilter
{
	public:
		std::vector<bufferRequest> getRequestedBuffers() override;
		bool init() override;
		void activate() override;
		void process(Npp8u *in, Npp32s inStep) override;
		void deactivate() override;
		std::string getName() override;

	private:
		Npp8u *gray = nullptr;
		Npp32s grayStep = 0;
		Npp16s *sobel16 = nullptr;
		Npp32s sobel16Step = 0;
		Npp8u *sobel8 = nullptr;
		Npp32s sobel8Step = 0;
};

class SobelH : public AFilter
{
	public:
		std::vector<bufferRequest> getRequestedBuffers() override;
		bool init() override;
		void activate() override;
		void process(Npp8u *in, Npp32s inStep) override;
		void deactivate() override;
		std::string getName() override;

	private:
		Npp8u *gray = nullptr;
		Npp32s grayStep = 0;
		Npp16s *sobel16 = nullptr;
		Npp32s sobel16Step = 0;
		Npp8u *sobel8 = nullptr;
		Npp32s sobel8Step = 0;
};

class SobelF : public AFilter
{
	public:
		std::vector<bufferRequest> getRequestedBuffers() override;
		bool init() override;
		void activate() override;
		void process(Npp8u *in, Npp32s inStep) override;
		void deactivate() override;
		std::string getName() override;

	private:
		Npp8u *gray = nullptr;
		Npp32s grayStep = 0;
		Npp16s *sobel16x = nullptr;
		Npp32s sobel16xStep = 0;
		Npp16s *sobel16y = nullptr;
		Npp32s sobel16yStep = 0;
		Npp8u *sobel8 = nullptr;
		Npp32s sobel8Step = 0;
};

class Gauss : public AFilter
{
	public:
		std::vector<bufferRequest> getRequestedBuffers() override;
		bool init() override;
		void activate() override;
		void process(Npp8u *in, Npp32s inStep) override;
		void deactivate() override;
		std::string getName() override;
};

class Sharpen : public AFilter
{
	public:
		std::vector<bufferRequest> getRequestedBuffers() override;
		bool init() override;
		void activate() override;
		void process(Npp8u *in, Npp32s inStep) override;
		void deactivate() override;
		std::string getName() override;
};

class PrewittHoriz : public AFilter
{
	public:
		std::vector<bufferRequest> getRequestedBuffers() override;
		bool init() override;
		void activate() override;
		void process(Npp8u *in, Npp32s inStep) override;
		void deactivate() override;
		std::string getName() override;
};

class PrewittVert : public AFilter
{
	public:
		std::vector<bufferRequest> getRequestedBuffers() override;
		bool init() override;
		void activate() override;
		void process(Npp8u *in, Npp32s inStep) override;
		void deactivate() override;
		std::string getName() override;
};

class PrewittFull : public AFilter
{
	public:
		std::vector<bufferRequest> getRequestedBuffers() override;
		bool init() override;
		void activate() override;
		void process(Npp8u *in, Npp32s inStep) override;
		void deactivate() override;
		std::string getName() override;

	private:
		Npp16s *origTo16s = nullptr;
		Npp32s origTo16sStep = 0;
		Npp16s *prewitt16x = nullptr;
		Npp32s prewitt16xStep = 0;
		Npp16s *prewitt16y = nullptr;
		Npp32s prewitt16yStep = 0;
};

class CannyBorderSobel : public AFilter
{
	public:
		std::vector<bufferRequest> getRequestedBuffers() override;
		bool init() override;
		void activate() override;
		void process(Npp8u *in, Npp32s inStep) override;
		void deactivate() override;
		std::string getName() override;

	private:
		Npp8u *firstBuff = nullptr;
		Npp32s firstBuffStep = 0;
		Npp8u *secondBuff = nullptr;
		Npp32s secondBuffStep = 0;
		Npp8u *cannyBuffer = nullptr;
		Npp8u *medianBuffer = nullptr;
		NppiSize medianMask = {3, 3};
		NppiPoint medianAnchor = {1, 1};
};

class RowNormalization : public AFilter
{
	public:
		std::vector<bufferRequest> getRequestedBuffers() override;
		bool init() override;
		void activate() override;
		void process(Npp8u *in, Npp32s inStep) override;
		void deactivate() override;
		std::string getName() override;

	private:
		Npp8u *gray = nullptr;
		Npp32s grayStep = 0;
		Npp32f *convertBuffer = nullptr;
		Npp32s convertBufferStep = 0;
		float *rowScales = nullptr;
};

struct HandComponentStats
{
	unsigned int pixelCount;
	int minX;
	int minY;
	int maxX;
	int maxY;
};

class HandScanning : public AFilter
{
	public:
		~HandScanning() override;

		std::vector<bufferRequest> getRequestedBuffers() override;
		bool init() override;
		void activate() override;
		void process(Npp8u *in, Npp32s inStep) override;
		void deactivate() override;
		std::string getName() override;

	private:
		bool handleBackgroundState(Npp8u *in, Npp32s inStep, NppiSize roi);
		bool generateForegroundMask(Npp8u *in, Npp32s inStep);
		bool closeForegroundMask(Npp8u *in, Npp32s inStep);
		bool labelComponents(NppiSize roi, int &maxLabelId);

		bool ensureComponentStatsCapacity(size_t requiredStatsCount);
		bool updateComponentStats(int maxLabelId);

		Npp32u findHandLabel(unsigned int &bestArea);
		bool extractHandMask(Npp32u label);

		void debugHand(Npp32u label, unsigned int area);

		void displayInput(Npp8u *in, Npp32s inStep, NppiSize roi);
		void displayMask(Npp8u *mask, Npp32s maskStep, NppiSize roi);

		Npp8u *bg = nullptr;
		Npp32s bgStep = 0;

		Npp8u *fgMask = nullptr;
		Npp32s fgMaskStep = 0;

		Npp8u *handMask = nullptr;
		Npp32s handMaskStep = 0;

		Npp8u *gaussian = nullptr;
		Npp32s gaussianStep = 0;

		Npp32u *labels = nullptr;
		Npp32s labelsStep = 0;

		Npp8u *labelWorkBuffer = nullptr;
		int labelWorkBufferSize = 0;
		int labelStartingNumber = 0;

		HandComponentStats *componentStatsDev = nullptr;
		size_t componentStatsCapacity = 0;

		std::vector<HandComponentStats> componentStatsHost;

		bool bg_valid = false;
		int bg_capture_countdown = -1;

		int debugTimer = 0;
};

std::vector<std::unique_ptr<AFilter>> createProcessingFilters();
bool initFilters(std::vector<std::unique_ptr<AFilter>>& filters);
