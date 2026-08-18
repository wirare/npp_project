#include <filter.hpp>

#include <iostream>
#include <memory>
#include <utility>
#include <vector>

std::vector<std::unique_ptr<AFilter>> createProcessingFilters()
{
	std::vector<std::unique_ptr<AFilter>> filters;

	filters.emplace_back(std::make_unique<PrewittFull>());
	filters.emplace_back(std::make_unique<SobelF>());
	filters.emplace_back(std::make_unique<CannyBorderSobel>());
	filters.emplace_back(std::make_unique<RowNormalization>());
	filters.emplace_back(std::make_unique<Empty>());
	filters.emplace_back(std::make_unique<HandScanning>());

	return filters;
}

bool initFilters(std::vector<std::unique_ptr<AFilter>>& filters)
{
	for (auto& filter : filters)
	{
		if (!filter->init())
		{
			std::cerr << "Failed to initialize filter: " << filter->getName() << "\n";
			return false;
		}
	}
	return true;
}
