#ifndef MAIN_STAGE_TYPES_H
#define MAIN_STAGE_TYPES_H

#include "types.h"

/// Parameters supplied by EVS command 20 when changing stage music.
/// The second halfword is retained command state; the current loader ignores it.
typedef struct StageMusicParams {
    u16 fadeFrames;
    u16 unusedCommandArg;
} StageMusicParams;

#endif // MAIN_STAGE_TYPES_H
