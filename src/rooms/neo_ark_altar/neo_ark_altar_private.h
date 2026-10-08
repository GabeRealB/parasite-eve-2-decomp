#ifndef SRC_ROOMS_NEO_ARK_ALTAR_NEO_ARK_ALTAR_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_ALTAR_NEO_ARK_ALTAR_PRIVATE_H

#include "types.h"

#include "gameplay/area_flags.h"

#include "main/task_types.h"

/// Two-entry spawn table: entry 0 is `_neoArkAltarLaunchMovieTask`, which
/// starts entry 1, the streaming task `_neoArkAltarPlayMovieTask`, on the
/// display list. The altar's cutscene driver and its task both spawn entry 0.
extern TaskDesc D_neo_ark_altar_8017EFC0[];

extern TaskDesc D_neo_ark_altar_8017F088[1];

extern AreaApplyRec D_neo_ark_altar_801800A0[3];

void func_neo_ark_altar_8017DC40(s32 arg0);

#endif // SRC_ROOMS_NEO_ARK_ALTAR_NEO_ARK_ALTAR_PRIVATE_H
