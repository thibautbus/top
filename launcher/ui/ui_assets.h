/* The launcher's embedded files: the vendored fonts (third_party/fonts/)
 * and the background motifs (motifs/), compiled into the binary by ui_embed.cmake. */
#ifndef ORACLES_UI_ASSETS_H
#define ORACLES_UI_ASSETS_H

#include <stddef.h>

typedef struct OraclesUiAsset {
    const char *name;
    const unsigned char *data;   /* followed by a NUL byte not counted in size */
    size_t size;
} OraclesUiAsset;

extern const OraclesUiAsset oracles_ui_assets[];
extern const unsigned oracles_ui_asset_count;

/* The asset of that name, or NULL. */
const OraclesUiAsset *oracles_ui_asset(const char *name);

#endif
