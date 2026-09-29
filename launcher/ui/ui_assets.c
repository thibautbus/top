#include "ui_assets.h"

#include <string.h>

const OraclesUiAsset *oracles_ui_asset(const char *name)
{
    for (unsigned i = 0; i < oracles_ui_asset_count; i++)
        if (!strcmp(oracles_ui_assets[i].name, name)) return &oracles_ui_assets[i];
    return NULL;
}
