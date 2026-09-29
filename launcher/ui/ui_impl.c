/* The implementations of the vendored single-header libraries
 * (third_party/stb, third_party/nanosvg), compiled once and
 * without the engine's warnings: their code is upstream's, not ours. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
