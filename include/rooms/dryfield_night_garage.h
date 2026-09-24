#ifndef ROOMS_DRYFIELD_NIGHT_GARAGE_H
#define ROOMS_DRYFIELD_NIGHT_GARAGE_H

#include "common.h"

#include "main/task.h"

/// Returns the task of the room work object whose id is the current area and
/// stage with `arg0` in bits 12 and up, or NULL when there is none.
Task* func_dryfield_night_garage_80180A64(s32 arg0);

#endif // ROOMS_DRYFIELD_NIGHT_GARAGE_H
