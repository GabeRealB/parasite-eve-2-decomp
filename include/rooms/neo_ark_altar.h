#ifndef ROOMS_NEO_ARK_ALTAR_H
#define ROOMS_NEO_ARK_ALTAR_H

#include "common.h"

#include "main/task.h"

/// Two-entry spawn table: entry 0 is `func_neo_ark_altar_8017DBF0`, which
/// starts entry 1, the streaming task `func_neo_ark_altar_8017DA40`, on the
/// display list. The altar's cutscene driver and its task both spawn entry 0.
extern TaskDesc D_neo_ark_altar_8017EFC0[];

void func_neo_ark_altar_8017DC40(s32 arg0);

#endif // ROOMS_NEO_ARK_ALTAR_H
