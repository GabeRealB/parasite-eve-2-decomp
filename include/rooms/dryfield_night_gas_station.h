#ifndef ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
#define ROOMS_DRYFIELD_NIGHT_GAS_STATION_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/ui.h"

/// One of the per-view objects the room's sprite-table record points at. Its
/// flags sit eight bytes apart -- one `GpSprtCmd` record each -- in a run
/// starting at 0x34. `func_dryfield_night_gas_station_80180DC8` writes the
/// three at 0x44 / 0x4C / 0x54 together, to 1 to light the lamp and to 0 to
/// darken it; `func_dryfield_night_gas_station_80180C3C` writes the two at
/// 0x34 / 0x3C, to 1 or to 0 with its own argument. The room carries several
/// of these, reached through the pointers below.
typedef struct DryfieldNightGasStationSprtView {
    /* 0x00 */ byte pad_0[0x34];
    /* 0x34 */ u8   field_34;
    /* 0x35 */ byte pad_35[0x7];
    /* 0x3C */ u8   field_3C;
    /* 0x3D */ byte pad_3D[0x7];
    /* 0x44 */ u8   field_44;
    /* 0x45 */ byte pad_45[0x7];
    /* 0x4C */ u8   field_4C;
    /* 0x4D */ byte pad_4D[0x7];
    /* 0x54 */ u8   field_54;
} DryfieldNightGasStationSprtView;
STATIC_ASSERT_SIZEOF(DryfieldNightGasStationSprtView, 0x55);

/// The record `Gp_SprtTables[stage - 1]->field_0[room - 1]` really points at:
/// a room-sized block, far larger than the 0xC-byte `GpSprtRec` the table's
/// element type declares, so the room reaches its tail through a cast (as the
/// water tower's `DwtwSprtRec` and the dumping hole's `SprtBigRec` do). The
/// tail is a run of pointers to `DryfieldNightGasStationSprtView`, twelve bytes
/// apart and starting at 0x34; `func_dryfield_night_gas_station_80180C3C`
/// reaches the 0x34 / 0x94 / 0xA0 / 0xAC / 0xC4 entries and
/// `func_dryfield_night_gas_station_80180DC8` the 0xA0 / 0xAC / 0xC4 ones.
typedef struct DryfieldNightGasStationSprtRec {
    /* 0x00 */ byte                             pad_0[0x34];
    /* 0x34 */ DryfieldNightGasStationSprtView* field_34;
    /* 0x38 */ byte                             pad_38[0x5C];
    /* 0x94 */ DryfieldNightGasStationSprtView* field_94;
    /* 0x98 */ byte                             pad_98[0x8];
    /* 0xA0 */ DryfieldNightGasStationSprtView* field_A0;
    /* 0xA4 */ byte                             pad_A4[0x8];
    /* 0xAC */ DryfieldNightGasStationSprtView* field_AC;
    /* 0xB0 */ byte                             pad_B0[0x14];
    /* 0xC4 */ DryfieldNightGasStationSprtView* field_C4;
} DryfieldNightGasStationSprtRec;
STATIC_ASSERT_SIZEOF(DryfieldNightGasStationSprtRec, 0xC8);

/// The block the room's effect task carries as its `spawnArg2`.
/// `func_dryfield_night_gas_station_80180E9C` keeps the spawn offset it hands
/// `Gp_SpawnEff` in `pos`, sets `active` once game flag nibble 0x63 has been
/// seen clear, and stores the per-anchor effect roll in `kind`. The bytes
/// around those fields are not reached here.
typedef struct DryfieldNightGasStationEffWork {
    byte    pad_0[0x10];
    SVECTOR pos;
    byte    pad_18[0xC];
    s16     active;
    s16     kind;
} DryfieldNightGasStationEffWork;

/// A `MATRIX` together with the word-wise view the room uses to reset it to an
/// identity rotation with five aligned stores instead of nine halfword ones.
typedef union DryfieldNightGasStationMatrix {
    MATRIX mat;
    struct {
        s32 m00_m01;
        s32 m02_m10;
        s32 m11_m12;
        s32 m20_m21;
        s16 m22;
    } ident;
} DryfieldNightGasStationMatrix;
STATIC_ASSERT_SIZEOF(DryfieldNightGasStationMatrix, 0x20);

/// The room's task descriptor table; its spawners pick an entry by index.
extern TaskDesc D_dryfield_night_gas_station_801888A0;

/// Handle of the task spawned from entry 0 of `D_dryfield_night_gas_station_801888A0`,
/// or NULL while none runs. `func_dryfield_night_gas_station_801807D4` either
/// passes it an argument or kills it.
extern Task* D_dryfield_night_gas_station_801907A4;

/// Handle of the task spawned from entry 1 or 3 of
/// `D_dryfield_night_gas_station_801888A0`, or NULL while none runs.
extern Task* D_dryfield_night_gas_station_801907A8;

/// UI descriptor of the help-line box (`func_dryfield_night_gas_station_8017ECF0`)
/// that the "Play Data" and usage panels open beside their lists.
extern UiObjectDesc D_dryfield_night_gas_station_80183FAC;

/// Panel titles drawn by the room's "Play Data" menu tasks.
extern const char D_dryfield_night_gas_station_8017D610[];
extern const char D_dryfield_night_gas_station_8017D624[];
extern const char D_dryfield_night_gas_station_8017D630[];
extern const char D_dryfield_night_gas_station_8017D638[];

/// The "100.0%" string drawn for a full-share usage row.
extern const u8 D_dryfield_night_gas_station_8017D61C[];

/// Exit callback of the help-line box task: releases `Wip_UiHolder` if
/// the task owns it, then frees the task's UI object and kills it.
void func_dryfield_night_gas_station_8017F3E0(Task* task);

/// Draws one semi-transparent gouraud capsule between the projections of the
/// world-space points `pos[0]` and `pos[1]`; `size` is a signed half-extent
/// scaled by depth and `rgb` a 4-bit-per-channel colour.
void func_dryfield_night_gas_station_801812B4(SVECTOR* pos, s32 size, s32 rgb);

/// Draws one semi-transparent textured quad centred on the projection of the
/// world-space point `pos`; `column` picks the texture column and `size` is a
/// signed half-extent scaled by depth.
void func_dryfield_night_gas_station_80181AF8(SVECTOR* pos, s32 column, s32 size);

/// Draws a gouraud ring of sixteen wedges around the projection of `coord`,
/// black at the half-extent `inner` and tinted by `rgb` at `inner + width`,
/// both scaled by depth.
void func_dryfield_night_gas_station_80182024(GsCOORDINATE2* coord, s32 inner, s32 width, u8* rgb);

/// Draws a gouraud disc of eight wedges around the projection of `coord`, lit
/// by `rgb` at the centre and fading to black; `radius` is a signed
/// half-extent scaled by depth.
void func_dryfield_night_gas_station_80182450(GsCOORDINATE2* coord, s32 radius, u8* rgb);

/// Draws a two-ring glow of gouraud wedges around the projection of `coord`,
/// tinted by `rgb`; `radius` is a signed half-extent scaled by depth.
void func_dryfield_night_gas_station_80183354(GsCOORDINATE2* coord, s16 radius, u8* rgb);

#endif // ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
