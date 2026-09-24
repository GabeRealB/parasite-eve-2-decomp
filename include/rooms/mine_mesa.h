#ifndef ROOMS_MINE_MESA_H
#define ROOMS_MINE_MESA_H

#include "common.h"

#include "gameplay/1A8.h"
#include "main/task.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Event parameters copied to the room's pending event. The room's event
/// state machine (`func_mine_mesa_8017D670`) runs the cap command in `field_0`
/// and the stage sound in `field_4`; `field_8` is the game flag checked and set
/// when the event starts, and `field_A` tells the state machine whether to
/// spawn its helper task.
typedef struct MineMesaEvent {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u8  field_A;
} MineMesaEvent;
STATIC_ASSERT_SIZEOF(MineMesaEvent, 0xC);

/// One entry of the lists a `MineMesaLayout` points at: an x/y/z triple
/// padded to 8 bytes.
typedef struct MineMesaVec {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
    /* 0x6 */ s16 pad;
} MineMesaVec;
STATIC_ASSERT_SIZEOF(MineMesaVec, 0x8);

/// A 12-byte record that is only ever copied whole, so its fields are unknown.
typedef struct MineMesaRecord {
    /* 0x0 */ u16 field_0[6];
} MineMesaRecord;
STATIC_ASSERT_SIZEOF(MineMesaRecord, 0xC);

/// Pointers into the room's layout data. The overlay holds a template
/// (`D_mine_mesa_801864A4`) and a live copy (`D_mine_mesa_8018700C`), which
/// `func_mine_mesa_8017EB54` fills from the template. `field_4` and `field_C`
/// have three entries each, and `field_8` has eight. The struct may extend
/// past `field_C`.
typedef struct MineMesaLayout {
    /* 0x0 */ s32             field_0;
    /* 0x4 */ MineMesaVec*    field_4;
    /* 0x8 */ MineMesaVec*    field_8;
    /* 0xC */ MineMesaRecord* field_C;
} MineMesaLayout;

extern MineMesaLayout D_mine_mesa_801864A4;
extern MineMesaLayout D_mine_mesa_8018700C;

extern GpSaveLoc     D_mine_mesa_80189B40;
extern s8            D_mine_mesa_80189B48;
extern MineMesaEvent D_mine_mesa_80189B60;

/// The room's task descriptor table; its spawners pick an entry by index.
extern TaskDesc D_mine_mesa_801842F4;

/// Handle of the task spawned from entry 1 or 3 of `D_mine_mesa_801842F4`, or
/// NULL while none runs.
extern Task* D_mine_mesa_80189B54;

/// Handle of the task spawned from entry 4 of `D_mine_mesa_801842F4`, or NULL
/// while none runs.
extern Task* D_mine_mesa_80189B5C;

/// Draws one semi-transparent textured quad centred on the projection of the
/// world-space point `pos`; `column` picks the texture column and `size` is a
/// signed half-extent scaled by depth.
void func_mine_mesa_8017EFA8(SVECTOR* pos, s32 column, s32 size);

/// Draws a gouraud ring of sixteen wedges around the projection of `coord`,
/// black at the half-extent `inner` and tinted by `rgb` at `inner + width`,
/// both scaled by depth.
void func_mine_mesa_8017F4D4(GsCOORDINATE2* coord, s32 inner, s32 width, u8* rgb);

/// Draws a gouraud disc of eight wedges around the projection of `coord`, lit
/// by `rgb` at the centre and fading to black; `radius` is a signed
/// half-extent scaled by depth.
void func_mine_mesa_8017F900(GsCOORDINATE2* coord, s32 radius, u8* rgb);

/// Draws a two-ring glow of gouraud wedges around the projection of `coord`,
/// tinted by `rgb`; `radius` is a signed half-extent scaled by depth.
void func_mine_mesa_80180804(GsCOORDINATE2* coord, s16 radius, u8* rgb);

#endif // ROOMS_MINE_MESA_H
