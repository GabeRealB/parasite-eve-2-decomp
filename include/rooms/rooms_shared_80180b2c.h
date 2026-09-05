#ifndef ROOMS_SHARED_80180B2C_H
#define ROOMS_SHARED_80180B2C_H

#include "common.h"

#include "main/task.h"

/// The head of the room's script work block, as far as this body is concerned.
/// Each carrying room names the whole block differently - `DgsWork`, `DwhWork`,
/// `DwtWork` - but all three agree on the two script parameters at 0x4, which
/// is all the shared object touches.
typedef struct RoomsShared80180b2cWork {
    /* 0x0 */ void* owner;
    /* 0x4 */ s16   field_4;
    /* 0x6 */ s16   field_6;
} RoomsShared80180b2cWork;

/// The room's script task, whose `idMap` holds the work block. Every carrying
/// room has its own pointer at its own address, named there by the family's
/// symbol maps.
extern Task* RoomsShared80180b2cTask;

/// Sets the room script's first parameter and clears the second.
void RoomsShared80180b2c(s16 arg0);

#endif // ROOMS_SHARED_80180B2C_H
