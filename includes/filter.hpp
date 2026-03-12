#pragma once

#include <npp.h>
#include <vector>

#define DEF_FILTER_FN(name) void do_##name##_on_image(Npp8u *in, int inStep)
#define FILTER_FN_NAME(name) (image_processing_struct){do_##name##_on_image, "Current filter : "#name}

DEF_FILTER_FN(SobelV);
DEF_FILTER_FN(SobelH);
DEF_FILTER_FN(SobelF);
DEF_FILTER_FN(Gauss);
DEF_FILTER_FN(Sharpen);
DEF_FILTER_FN(PrewittHoriz);
DEF_FILTER_FN(PrewittVert);
DEF_FILTER_FN(PrewittFull);
DEF_FILTER_FN(CannyBorderSobel);

typedef void(*image_processing_fn)(Npp8u *, int);
typedef struct {image_processing_fn fn; const char *name;} image_processing_struct;

static const std::vector<image_processing_struct> processing_functions = 
{
	FILTER_FN_NAME(PrewittFull),
	FILTER_FN_NAME(SobelF),
	FILTER_FN_NAME(CannyBorderSobel),
};

