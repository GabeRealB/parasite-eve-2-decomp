#ifndef ROOMS_ACROPOLIS_SQUARE_H
#define ROOMS_ACROPOLIS_SQUARE_H

#include "common.h"

#include "rooms/room_common.h"

/// Work block the square's cutscene task (`func_acropolis_square_80181228`,
/// entry 0 of `D_acropolis_square_801837A0`) reads through `Task::spawnArg2`.
/// The task walks its own states and pulls one field per state out of this
/// block, so the callers that spawn it only fill in the fields their scene
/// needs and leave the rest at whatever the previous scene wrote.
///
/// `camera` is signed: a positive value is latched into `Mc_SaveData.field_4`
/// (the previous camera is kept in `D_80115694` so it can be restored), a
/// negative one only parks its negation there. `capFile` is the `Gp_LoadCapFile`
/// id the scene streams, with `fadeArg0` / `fadeArg1` the `func_800E6D4C`
/// arguments used when it loads (`0x3C0, 0` when `fadeArg0` is zero). The four
/// `snd*` words are `SndEvt_EnqueueType6` ids from the square's own bank
/// (`0x5101xxxx`), fired at the start, on the two prompt buttons and on exit.
typedef struct AcropolisSquareCutscene {
    /* 0x00 */ s8   camera;
    /* 0x01 */ s8   field_1;
    /* 0x02 */ s8   field_2;
    /* 0x03 */ s8   capFile;
    /* 0x04 */ s32  sndStart;
    /* 0x08 */ s32  snd8;
    /* 0x0C */ s32  sndC;
    /* 0x10 */ s32  snd10;
    /* 0x14 */ s16  fadeArg0;
    /* 0x16 */ s16  fadeArg1;
    /* 0x18 */ byte pad_18[0x8];
} AcropolisSquareCutscene;
STATIC_ASSERT_SIZEOF(AcropolisSquareCutscene, 0x20);

#endif // ROOMS_ACROPOLIS_SQUARE_H
