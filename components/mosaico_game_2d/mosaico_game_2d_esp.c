// SPDX-License-Identifier: Apache-2.0
/* Keep the ESP log implementation in the renderer object: a separate archive
 * member would not be extracted because debug supplies a weak fallback. */
#include "mosaico_game_2d.c"
#include "raster_log.c"
