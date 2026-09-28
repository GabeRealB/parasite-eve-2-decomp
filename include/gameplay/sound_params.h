#ifndef GAMEPLAY_SOUND_PARAMS_H
#define GAMEPLAY_SOUND_PARAMS_H

#include "common.h"

/// Two halfwords at `D_8007A39C`. `Gp_EndingTask` zeros both before spawning
/// the bank-load task. `field_0` is the u16 sound param used by
/// `Task_AllocIdMap`.
typedef struct _GpSndParam {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} GpSndParam;
STATIC_ASSERT_SIZEOF(GpSndParam, 4);

#endif // GAMEPLAY_SOUND_PARAMS_H
