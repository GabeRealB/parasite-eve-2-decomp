#ifndef ACTOR_105700_H
#define ACTOR_105700_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/sound.h"
#include "main/task.h"

/// 0x6D8-byte work block hung off `Actor105700.field_1C`. It opens with the
/// animation context and its nineteen 0x28-byte slots, exactly like the
/// `Actor02000Work` block of `actor_102000`; only the sound-flags halfword
/// at 0x6A0 and the animation index at 0x6D6 are modelled so far.
typedef struct Actor105700Work {
    /* 0x000 */ GpAnimCtx ctx;
    /* 0x014 */ byte      slots[19][0x28];
    /* 0x30C */ byte      pad_30C[0x394];
    /* 0x6A0 */ u16       field_6A0; ///< sound flags; bit 5/4 gate the two cues
    /* 0x6A2 */ byte      pad_6A2[0x30];
    /// Spawn state driven by `func_actor_105700_80137130`: 0 clears the
    /// coordinate, 1 fires the effect burst and sound cue, 2 is idle.
    /* 0x6D2 */ s16  field_6D2;
    /* 0x6D4 */ byte pad_6D4[2];
    /* 0x6D6 */ s16  field_6D6; ///< animation index, used as a table row
} Actor105700Work;
STATIC_ASSERT_SIZEOF(Actor105700Work, 0x6D8);

/// Spawn/context block behind `Actor105700.field_20`; `field_8` is the
/// halfword the sound id takes its room/channel bits from.
typedef struct Actor105700Ctx {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u16  field_8;
} Actor105700Ctx;

/// Model object behind `Actor105700.field_2C`; `field_8` is the trailing
/// per-part coordinate array the pan/depth helpers read. `field_C` is the
/// halfword `func_actor_105700_80137130` mirrors from the owner's object.
typedef struct Actor105700Obj {
    /* 0x00 */ byte     pad_0[8];
    /* 0x08 */ GpObj38* field_8;
    /* 0x0C */ u16      field_C;
} Actor105700Obj;

typedef struct Actor105700 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor105700Work* field_1C;
    /* 0x20 */ Actor105700Ctx*  field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor105700Obj*  field_2C;
} Actor105700;

void func_actor_105700_801336FC(Actor105700* arg0);

#endif
