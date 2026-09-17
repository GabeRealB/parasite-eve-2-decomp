#ifndef ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H
#define ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H

#include "common.h"

#include "main/task.h"

/// Parameter block of `func_neo_ark_eve_access_tunnel_8017D6D4`, the room-local
/// resolver `func_neo_ark_eve_access_tunnel_8017D980` calls with one pointer as
/// both its input and its output.
///
/// `field_0` is the code the resolver switches on, `field_2` passes through
/// unchanged, `field_3` is the byte it writes, and a non-zero `field_5` makes it
/// return without touching anything. The caller stages the block from the
/// `NaetEventDesc` it is about to publish and copies `field_3` back into it.
typedef struct NaetUtilParam {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4;
    /* 0x5 */ u8  field_5;
} NaetUtilParam;
STATIC_ASSERT_SIZEOF(NaetUtilParam, 0x6);

/// The twelve bytes `func_neo_ark_eve_access_tunnel_8017D980` stages in
/// `D_neo_ark_eve_access_tunnel_801807A8` before spawning the tunnel's outgoing
/// task from `D_neo_ark_eve_access_tunnel_8017EA88`.
///
/// The task that consumes it is the neighbouring start-up sequence,
/// `func_neo_ark_eve_access_tunnel_8017D810`: `field_4` is the halfword it
/// forwards as message 0x3EE and gives up on when it reads 0xFFFF, `field_8` the
/// sound event it queues and polls, and the four bytes at 0x0 are the
/// save-location block it copies into `Mc_SaveData`'s 0x5..0x8. `field_1` is the
/// code `func_neo_ark_eve_access_tunnel_8017D980` hands its resolver, staged
/// from the task's `spawnArg1`.
///
/// `field_6` is never read or written by either side, so it is padding.
typedef struct NaetEventDesc {
    /* 0x0 */ u8   field_0;
    /* 0x1 */ u8   field_1;
    /* 0x2 */ u8   field_2;
    /* 0x3 */ u8   field_3;
    /* 0x4 */ u16  field_4;
    /* 0x6 */ byte pad_6[0x2];
    /* 0x8 */ s32  field_8;
} NaetEventDesc;
STATIC_ASSERT_SIZEOF(NaetEventDesc, 0xC);

/// One of the per-view objects the tunnel's sprite-table record points at. Its
/// flag sits at a different offset in each of the record's three pointers --
/// 0x24 for `field_1C`, 0x1C for `field_28` and 0x14 for `field_34`, eight
/// bytes apart, one `GpSprtCmd` record each, the same shape the gas station's
/// `DryfieldNightGasStationSprtView` has. `func_neo_ark_eve_access_tunnel_8017E090`
/// is the only writer: it sets the flag to 1 or clears it to 0.
typedef struct NaetSprtView {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x7];
    /* 0x1C */ u8   field_1C;
    /* 0x1D */ byte pad_1D[0x7];
    /* 0x24 */ u8   field_24;
} NaetSprtView;
STATIC_ASSERT_SIZEOF(NaetSprtView, 0x25);

/// The record `Gp_SprtTables[stage - 1]->field_0[room - 1]` really points at,
/// reached through a cast for the same reason the neighbouring rooms' records
/// are: it is a room-sized block, far larger than the 0xC-byte `GpSprtRec` the
/// table's element type declares. `field_1C` / `field_28` / `field_34` are a
/// run twelve bytes apart, one `GpSprtRec` each, and each points at one of the
/// views above -- the first alone, the second and third as a pair.
typedef struct NaetSprtRec {
    /* 0x00 */ byte          pad_0[0x1C];
    /* 0x1C */ NaetSprtView* field_1C;
    /* 0x20 */ byte          pad_20[0x8];
    /* 0x28 */ NaetSprtView* field_28;
    /* 0x2C */ byte          pad_2C[0x8];
    /* 0x34 */ NaetSprtView* field_34;
} NaetSprtRec;
STATIC_ASSERT_SIZEOF(NaetSprtRec, 0x38);

/// Sets the tunnel's view flags: `arg0` picks the run to write (0 the single
/// view `field_1C` carries, 1 the `field_28` / `field_34` pair) and `arg1`
/// picks the value -- 0 sets each flag of that run to 1, 1 clears it to 0 --
/// while any other pair of arguments writes nothing.
/// `func_neo_ark_eve_access_tunnel_8017DFC0` calls it with (0, 0) and (1, 0)
/// together while the session is still below state 4, so both runs are set.
void func_neo_ark_eve_access_tunnel_8017E090(s32 arg0, s32 arg1);

/// Descriptor the tunnel's outgoing task is spawned from, index 0 of the table
/// `func_neo_ark_eve_access_tunnel_8017D980` hands `Task_SpawnFromTable`.
extern TaskDesc D_neo_ark_eve_access_tunnel_8017EA88;

/// The staged event descriptor, read by the task spawned above.
extern NaetEventDesc D_neo_ark_eve_access_tunnel_801807A8;

/// Resolves the code in `in->field_0` and writes the resulting byte to
/// `out->field_3`.
void func_neo_ark_eve_access_tunnel_8017D6D4(NaetUtilParam* in, NaetUtilParam* out);

#endif // ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H
