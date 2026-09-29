/* Diagnostic panel: the hook table armed on the running game,
 * every event of docs/GAME_HOOKS.md's section 6 printed as it happens,
 * with what the game passes in its registers, so that the player can check
 * that the panel tells the truth about what they do. */
#ifndef ORACLES_DIAGNOSTICS_H
#define ORACLES_DIAGNOSTICS_H

#include "guest.h"

#include <stdint.h>
#include <stdio.h>

typedef struct OraclesDiagnostics OraclesDiagnostics;

/* The guest stays owned by the caller; the panel becomes its event sink. */
OraclesDiagnostics *oracles_diagnostics_start(OraclesGuest *guest, FILE *out);
void oracles_diagnostics_stop(OraclesDiagnostics *diag);

/* Host frame hooks: pass as on_frame_begin / on_frame_end with the panel as opaque. */
void oracles_diagnostics_frame_begin(void *opaque, uint32_t frame);
void oracles_diagnostics_frame_end(void *opaque, uint32_t frame);

#endif
