#ifndef MAIN_SOUND_TYPES_H
#define MAIN_SOUND_TYPES_H

#include "common.h"

/// 0x10-byte linear interpolator state used by LinInterp_Setup / LinInterp_Apply /
/// LinInterp_Step. Embedded at MidiSong::field_14 and in the CD audio player's state.
typedef struct _LinInterp {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s32 field_8;
    /* 0xC */ s16 field_C;
    /* 0xE */ s16 field_E;
} LinInterp;
STATIC_ASSERT_SIZEOF(LinInterp, 0x10);

#endif // MAIN_SOUND_TYPES_H
