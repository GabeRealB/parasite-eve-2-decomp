#ifndef GAMEPLAY_3CD8_H
#define GAMEPLAY_3CD8_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/session.h"
#include "main/task.h"

struct _GpEffWork;

/// 0xC-byte sequence-table record. `Gp_CapTable` is the current table
/// (`Gp_StartCap` stores its first arg there). `Gp_FindCapEvt` walks
/// from a start index until `field_8 == -1` (terminator) or `field_5`
/// equals `Gp_CapEventKey` (the key `Gp_StartCap` saved from its third arg).
/// When not -1, `field_8` is a relocated `u16*` text stream walked by
/// `Gp_CapTextTopY` / `Gp_CapTextHeight` / `func_800E6BB8` / `Gp_CapCenterX` / `Gp_CapCenterXLine` (codes `-1` end,
/// `-2` newline, `-3` skip; else glyph index `& 0x3FF` into `Gp_CapGlyphs`).
typedef struct _GpEvt12 {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4; // flags copied to D_80115670; bit 0 cleared if field_7
    /* 0x5 */ u8  field_5; // compared with Gp_CapEventKey
    /* 0x6 */ u8  field_6;
    /* 0x7 */ u8  field_7; // copied to D_80115678
    /* 0x8 */ s32 field_8; // -1 terminator, else relocated u16* text
} GpEvt12;
STATIC_ASSERT_SIZEOF(GpEvt12, 0xC);

/// 0x10-byte header in front of a `GpEvt12` array inside a `GpCapFile`
/// (`field_C`). `count` is the first halfword; the records start at
/// `hdr + 1`. `Gp_RelocCapFile` relocates each record's `field_8` unless
/// it is `-1`, in which case it also skips the next record.
typedef struct _GpCapEvtTable {
    /* 0x00 */ s16  count;
    /* 0x02 */ byte pad_2[0xE];
} GpCapEvtTable;
STATIC_ASSERT_SIZEOF(GpCapEvtTable, 0x10);

/// Count word in front of the relocated pointer table at `GpCapFile::field_10`.
/// `Gp_RelocCapFile` publishes `hdr + 1` as `Gp_CapCmds` (`GpCapCmd**`).
typedef struct _GpCapPtrTable {
    /* 0x0 */ s32 count;
} GpCapPtrTable;
STATIC_ASSERT_SIZEOF(GpCapPtrTable, 4);

/// Command record pointed to by `Gp_CapCmds[index]`. `Gp_RunCapCmd` switches
/// on `field_0` and may follow `field_8` to another index. Flag id is
/// `field_3 | (field_7 << 8)`. `field_1` bits: 0 = wrap counter, 1 = persist
/// counter in a game-flag nibble, 2 = skip/compare against `field_2`.
/// `Gp_StartCapSlot` then starts the event at the same table slot.
typedef struct _GpCapCmd {
    /* 0x0 */ u8 field_0; // opcode (0..4)
    /* 0x1 */ u8 field_1; // flags
    /* 0x2 */ u8 field_2; // counter limit
    /* 0x3 */ u8 field_3; // flag id lo
    /* 0x4 */ u8 field_4; // live counter
    /* 0x5 */ u8 field_5; // first 2-bit slot (`Gp_GetCurBit2Flag`)
    /* 0x6 */ u8 field_6; // slot count
    /* 0x7 */ u8 field_7; // flag id hi
    /* 0x8 */ u8 field_8; // next command index
} GpCapCmd;

/// In-memory CAP dialogue file (`strncmp` magic `"CAP"`). Offsets at
/// `field_8` / `field_C` / `field_10` are file-relative until
/// `Gp_RelocCapFile` adds the file base. After that, `field_8` is a
/// `GlyphUvwh*` published as `Gp_CapGlyphs`, `field_C` is a
/// `GpCapEvtTable*`, and `field_10` is a `GpCapPtrTable*` whose
/// entries (nonzero) are relocated `GpEvt12*` values.
typedef struct _GpCapFile {
    /* 0x00 */ char magic[4];
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;  // glyph table offset / GlyphUvwh*
    /* 0x0C */ s32  field_C;  // event table offset / GpCapEvtTable*
    /* 0x10 */ s32  field_10; // pointer table offset / GpCapPtrTable*
} GpCapFile;
STATIC_ASSERT_SIZEOF(GpCapFile, 0x14);

/// Three overlay ids passed through `func_800E7498` (table cmd 0xFA6)
/// to `CdCmd_StartOverlay`. Also the current evs triple held in
/// `D_801156F4` (`Gp_CapExit` formats `"evs%d_%d_%d.txt"` from it).
typedef struct _GpOverlayIds {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
} GpOverlayIds;
STATIC_ASSERT_SIZEOF(GpOverlayIds, 6);

/// 0x14-byte payload copied from `Gp_WeaponMsgRec` by `Gp_MsgPlayerWeapon` (arg 0)
/// and sent as slot-3 msg `0x3E8`. `field_0` is overwritten with
/// `Gp_WeaponIdBase[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21`
/// (same value `Gp_PlayerWeaponId` writes). `Gp_MsgAllyWeapon` copies the same
/// record to slot 0xA and overwrites `field_0` with
/// `Gp_AllyIdBase[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7`
/// (same value `Gp_AllyAnimId` writes). Nearby `D_8010FB10` /
/// `D_8010FB24` are the same size (`Gp_EvtCapWeaponTask` copies them for msg `0x3FA`).
typedef struct _GpRec14 {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} GpRec14;
STATIC_ASSERT_SIZEOF(GpRec14, 0x14);

/// 4-byte volume-fade payload at `Task::spawnArg2` for `Gp_VolFadeTask`.
/// `field_0` is the target volume passed to `Snd_ApplyVolumeTable`.
/// `field_2` is the fade duration in frames (`0` applies immediately).
typedef struct _GpVolFade {
    /* 0x0 */ u16 field_0; // target volume
    /* 0x2 */ u16 field_2; // duration
} GpVolFade;
STATIC_ASSERT_SIZEOF(GpVolFade, 4);

/// 0xC-byte Type-A sound-param fade at `Task::spawnArg2` for `Gp_SndFadeTask`
/// (bank 9 type 0xE; live instance `D_801156E0`). `field_0` is the sound id
/// passed to `SndEvt_EnqueueTypeA`. `field_4` is the start/current param
/// (snapshotted into `D_801156C4`); `field_6` is the target; `field_8` is
/// the duration in frames (`0` applies `field_6` immediately). Completing
/// or instant-applying the fade clears `D_8010FBE8`.
typedef struct _GpSndFade {
    /* 0x0 */ s32  field_0; // sound id
    /* 0x4 */ u16  field_4; // start / current param
    /* 0x6 */ u16  field_6; // target param
    /* 0x8 */ u16  field_8; // duration
    /* 0xA */ byte pad_A[2];
} GpSndFade;
STATIC_ASSERT_SIZEOF(GpSndFade, 0xC);

/// Packed bytes in `Task::spawnArg1` for `Gp_DelayedMsgTask`.
/// `field_0` is forwarded as a2 to `Gp_DispatchMsg`.
/// `field_1` is copied into `Task::killCountdown` on state 0.
/// `field_2` selects the target: 0 = slot 3, 1 = slot 0xA, else
/// `Gp_LookupSlot4(field_2 - 2)`.
typedef struct _GpSpawnArg {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} GpSpawnArg;
STATIC_ASSERT_SIZEOF(GpSpawnArg, 4);

/// Object stored in `Task::spawnArg2` for `Gp_EndWaitTask`. `field_2` is a
/// signed completion flag: when non-zero the task calls `Stage_SetEndingFlag`
/// and kills itself.
typedef struct _GpEndWait {
    /* 0x00 */ byte pad_0[2];
    /* 0x02 */ s8   field_2;
} GpEndWait;

/// 4-byte dual-script command. `GpState34::field_0` is an array of these.
/// Script A reads `field_0`, script B reads `field_2`. Low byte is the opcode
/// (0 = stop, 1 = timed pad from `field_4`, 2 = set delay, 3 = set/decrement
/// loop, 4 = loop jump); high byte is the payload.
typedef struct _GpScriptCmd {
    /* 0x0 */ u16 field_0; // script A command
    /* 0x2 */ u16 field_2; // script B command
} GpScriptCmd;
STATIC_ASSERT_SIZEOF(GpScriptCmd, 4);

/// 4-byte pad record. `GpState34::field_4` is an array of these, indexed by
/// the high byte of an opcode-1 command. `field_2` is the delay copied to
/// `field_10` / `field_11`; `field_0` / `field_1` are start/end for script B.
typedef struct _GpScriptRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2; // delay
    /* 0x3 */ u8 field_3;
} GpScriptRec;
STATIC_ASSERT_SIZEOF(GpScriptRec, 4);

/// 0x34-byte dual-script state allocated by `Gp_ScriptInit` (`Mem_Calloc(0x34, 0)`)
/// and stored on the owner task at +0x1C (`Task::idMap`).
/// `field_10` / `field_11` are delay counters for scripts A / B.
typedef struct _GpState34 {
    /* 0x00 */ GpScriptCmd* field_0;  // script table (from Task::spawnArg2)
    /* 0x04 */ GpScriptRec* field_4;  // secondary pad table
    /* 0x08 */ s16          field_8;
    /* 0x0A */ u16          field_A;  // current command (script A)
    /* 0x0C */ u16          field_C;  // current command (script B)
    /* 0x0E */ u8           field_E;  // script A PC
    /* 0x0F */ u8           field_F;  // script B PC
    /* 0x10 */ u8           field_10; // delay A
    /* 0x11 */ u8           field_11; // delay B
    /* 0x12 */ u8           field_12;
    /* 0x13 */ u8           field_13;
    /* 0x14 */ u8           field_14; // loop counter A
    /* 0x15 */ u8           field_15; // loop counter B
    /* 0x16 */ byte         pad_16[0x12];
    /* 0x28 */ s32          field_28;
    /* 0x2C */ s32          field_2C;
    /* 0x30 */ s32          field_30;
} GpState34;
STATIC_ASSERT_SIZEOF(GpState34, 0x34);

/// 0xC-byte interpolator state allocated by `Gp_SpawnPadLerp` / `Gp_SpawnPadLerpScaled`
/// (`Mem_Calloc(0xC, 0)`) and stored at `Task::idMap` for bank-2 type 0xC.
/// `field_8` is the duration; `field_4.as_s32` is start<<8; `field_0` is the
/// per-frame step `((end<<8) - (start<<8)) / duration`.
/// `Gp_PadLerpTask` posts `field_4.bytes.as_u8` (the 8-bit interpolator,
/// `as_s32 >> 8` on little-endian) via `Pad_PostEvent`.
typedef struct _GpState0C {
    /* 0x0 */ s32 field_0; // step
    /* 0x4 */ union {
        s32 as_s32;        // start << 8
        struct {
            /* 0x4 */ u8 pad_4;
            /* 0x5 */ u8 as_u8; // (as_s32 >> 8)
        } bytes;
    } field_4;
    /* 0x8 */ s16  field_8; // duration
    /* 0xA */ byte pad_A[2];
} GpState0C;
STATIC_ASSERT_SIZEOF(GpState0C, 0xC);

/// 0x18-byte work block allocated by `Gp_SpawnScript18` / `Gp_SpawnScript18Ex`
/// (`Mem_Calloc(0x18)`) and stored at `Task::idMap` for bank-2 type 0xD.
/// `Gp_SpawnScript18` writes `field_0`/`field_4` from its args and clears
/// `field_8`; `Gp_SpawnScript18Ex` writes all three. `Gp_DispatchScript18` indexes
/// dispatch tables with `field_A` and `field_C`.
typedef struct _GpState18 {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s16  field_8;
    /* 0x0A */ u8   field_A; // dispatch index A (Gp_DispatchScript18)
    /* 0x0B */ u8   field_B;
    /* 0x0C */ u8   field_C; // dispatch index B (Gp_DispatchScript18)
    /* 0x0D */ byte pad_D[0xB];
} GpState18;
STATIC_ASSERT_SIZEOF(GpState18, 0x18);

/// 0x1C-byte halfword state allocated by `Gp_InitState1C` (`Mem_Calloc(0x1C, 0)`)
/// and stored in `Gp_State1C` (also written to the owner task at +0x1C).
/// `Gp_InitState1C` sets `field_6` to 1 and the rest to 0.
typedef struct _GpState1C {
    /* 0x00 */ s16 field_0;
    /* 0x02 */ s16 field_2;
    /* 0x04 */ s16 field_4;
    /* 0x06 */ s16 field_6;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ s16 field_C;
    /* 0x0E */ s16 field_E;
    /* 0x10 */ s16 field_10; // flags (bit 0 checked by func_800EC9C8, bit 0x80 by func_800ECA54)
    /* 0x12 */ s16 field_12; // flags (bit 0x200 by Gp_EffCtlTaskAC, bit 0x400 by Gp_EffCtlTaskF3, bit 0x800 by Gp_EffCtlTask0E / func_800ECA54)
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
    /* 0x18 */ s16 field_18; // PE bit written by Gp_SetState1CPe
    /* 0x1A */ u16 field_1A; // flags (0x80 by Gp_PulseState1C80, 0x100 by Gp_PulseState1C)
} GpState1C;
STATIC_ASSERT_SIZEOF(GpState1C, 0x1C);

/// Overlay of `GpCoord64.coord` plus the 0x10-byte tail. `Gp_EffCtlTask6B`
/// holds `&Gp_RoomCoords[0].coord` as this type so `field_50` / `field_58`
/// are addressed from the coordinate pointer (`s5 + 0x50` / `s5 + 0x58`).
typedef struct _GpCoordTail {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ s16           field_50;
    /* 0x52 */ s16           field_52;
    /* 0x54 */ s16           field_54;
    /* 0x56 */ s16           pad_56;
    /* 0x58 */ s32           field_58;
    /* 0x5C */ s32           field_5C;
} GpCoordTail;
STATIC_ASSERT_SIZEOF(GpCoordTail, 0x60);

/// 0x64-byte world-coord slot. `Gp_InitRoomCoords` inits all 8 entries of
/// `Gp_RoomCoords`: `coord.sub` is the parent (`&Gfx_ViewCoord`) and `field_0`
/// is a refcount (decremented by `Gp_DecRoomCoordRefs`). `Gp_CountRoomCoords` returns
/// how many slots currently have a non-zero refcount. `Gp_EffCtlTask6B`
/// copies the actor translation into `coord`, writes `0xC00` into
/// `field_54` / `field_56` / `field_58`, and decays `field_5C` by `0x190`
/// while it is `>= 0x191`.
typedef struct _GpCoord64 {
    /* 0x00 */ s32           field_0;
    /* 0x04 */ GsCOORDINATE2 coord;
    /* 0x54 */ s16           field_54;
    /* 0x56 */ s16           field_56;
    /* 0x58 */ s16           field_58;
    /* 0x5A */ s16           pad_5A;
    /* 0x5C */ s32           field_5C;
    /* 0x60 */ s32           field_60;
} GpCoord64;
STATIC_ASSERT_SIZEOF(GpCoord64, 0x64);

/// 0x10-byte scratch from `G_SCRATCH_HEAD` used by `Gp_TraceGroundCoord` and
/// `func_800EA1A8`. `pos` is the low halves of the source XYZ. `dir`
/// starts as `(0, 0x1000, 0)`, is rotated by `Gfx_ViewWorldMtx`, then added
/// onto `pos` and passed to `func_800DE7CC`.
typedef struct _GpRayScratch {
    /* 0x0 */ SVECTOR pos;
    /* 0x8 */ SVECTOR dir;
} GpRayScratch;
STATIC_ASSERT_SIZEOF(GpRayScratch, 0x10);

/// 0x18-byte scratch from `G_SCRATCH_HEAD` used by `Gp_DrawRing`.
/// `vec` is the coordinate's `workm.t[]` truncated to s16 and fed to
/// `gte_ldv0`. `otz` is `gte_stszotz` (then incremented so it can also be
/// used as the divisor), `flag` is `gte_stflg` and `sx` / `sy` are the
/// `gte_stsxy` of the single RTPS. `step` is the per-vertex radius
/// `(arg1 * 64) / otz` swept around the ring by `rsin` / `rcos`.
typedef struct _GpRingScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     step;
    /* 0x14 */ s16     sx;
    /* 0x16 */ s16     sy;
} GpRingScratch;
STATIC_ASSERT_SIZEOF(GpRingScratch, 0x18);

/// 0x1C-byte scratch from `G_SCRATCH_HEAD` used by `Gp_DrawFxQuad`. Same
/// shape as `GpRingScratch` (the coordinate's `workm.t[]` truncated to s16,
/// fed to `gte_ldv0`, then `gte_stszotz` / `gte_stflg` / `gte_stsxy` of the
/// single RTPS), but the ring radius is not cached: instead `dx` / `dy` hold
/// the current `(arg2 * 31 / otz) * rsin|rcos(angle) >> 12` offsets that are
/// added to / subtracted from `sx` / `sy` to build the four quad corners.
/// Only the low halves of `dx` / `dy` are read back.
typedef struct _GpFxQuadScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} GpFxQuadScratch;
STATIC_ASSERT_SIZEOF(GpFxQuadScratch, 0x1C);

/// 0x1C-byte scratch from `G_SCRATCH_HEAD` used by `Gp_DrawArc`. Same
/// projection preamble as `GpRingScratch` (the coordinate's `workm.t[]`
/// truncated to s16, fed to `gte_ldv0`, then `gte_stszotz` / `gte_stflg` /
/// `gte_stsxy` of the single RTPS), but two radii are cached instead of one:
/// `inner` is `(arg1 * 64) / otz` and `outer` is `((arg1 + arg2) * 64) / otz`.
/// Each of the 16 `POLY_G4` segments spans both radii over a 0x100 arc.
typedef struct _GpArcScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     inner;
    /* 0x14 */ s32     outer;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} GpArcScratch;
STATIC_ASSERT_SIZEOF(GpArcScratch, 0x1C);

/// 0x118-byte scratch from `G_SCRATCH_HEAD` used by `Gp_DrawBandEx`. Holds
/// the two 16-vertex rings of a shaded band: `inner[i]` is the ring of
/// radius `arg1` and `outer[i]` the ring of radius `arg1 + arg2`, both built
/// in the XZ plane by `rsin` / `rcos`, rotated by the coordinate's `workm`
/// and offset by its `workm.t[]`. The second pass projects each segment:
/// `sxy0` is the `gte_stsxy` of `inner[i]` and `sxy1` / `sxy2` / `sxy3` the
/// `gte_stsxy3` of `inner[i + 1]` / `outer[i]` / `outer[i + 1]`, with `otz`
/// from `gte_stszotz` (then incremented) and `flag` from `gte_stflg`.
typedef struct _GpBandScratch {
    /* 0x000 */ SVECTOR inner[16];
    /* 0x080 */ SVECTOR outer[16];
    /* 0x100 */ s32     otz;
    /* 0x104 */ s32     flag;
    /* 0x108 */ DVECTOR sxy0;
    /* 0x10C */ DVECTOR sxy1;
    /* 0x110 */ DVECTOR sxy2;
    /* 0x114 */ DVECTOR sxy3;
} GpBandScratch;
STATIC_ASSERT_SIZEOF(GpBandScratch, 0x118);

extern GpState1C*    Gp_State1C;
extern Task*         Gp_State1CTask;
extern GpCoord64     Gp_RoomCoords[8];
extern GsCOORDINATE2 Gfx_ViewCoord;
/// Six CLUT X coordinates (0x20, 0x30, 0xC0, 0xD0, 0xE0, 0xF0) selected by
/// the top nibble of `Gp_DrawFxQuad`'s angle argument and paired with CLUT
/// Y 0x10B.
extern u16 Gp_QuadClutX[];
/// 8 packed RGB-nibble colors. Index is `cln(spawnArg1 << 12) / 2839 & 7`.
/// High nibble is the `Gp_DrawFadeQuad` blend; low three nibbles are R, G, B.
extern u16 Gp_FadeQuadColors[];

Task* Gp_SpawnScript18(s32 arg0, s32 arg1);
s32   Gp_LookupSlot4(s32 arg0);
void  Gp_RunCapCmd(s32 arg0, s16 arg1);
void  Gp_EvtCapWeaponTask(Task* arg0);
s32   Gp_StartCapSlot(s16 arg0, s16 arg1, s16 arg2);
void  Gp_MsgPlayer3F3(s32 arg0);
void  Gp_MsgPlayerWeapon(s32 arg0);
void  Gp_SpawnIfCapIdle(s32 arg0, s32 arg1);
/// Enqueues a type-6 sound event, substituting the current stage number into
/// the packed id when its stage nibble is set. `arg1` / `arg2` are the pan and
/// volume bytes.
void Gp_EnqueueStageSnd6(s32 arg0, s32 arg1, s32 arg2);
void Gp_MsgAllyWeapon(s32 arg0);
void Gp_ClearAllFlagNibbles(void);
void Gp_SpawnEvt1(s32 arg0, s32 arg1);
s32  Gp_RelocCapFile(GpCapFile* file);
s32  Gp_StartCap(s32 arg0, s16 arg1, s16 arg2);
/// Blinking POLY_G3 continue caret. `Gp_CapCaretDelay` is a frame delay before the
/// first draw; `Gp_CapCaretX` / `Gp_CapCaretY` are base XY; `Gp_CapCaretGrey` /
/// `Gp_CapCaretDir` pulse the vertex greys between 8 and 15.
void Gp_DrawCapCaret(void);
s16  Gp_CapCenterX(u16* arg0);
s16  Gp_CapCenterXLine(u16* arg0, s32 arg1);
s16  Gp_CapTextHeight(u16* arg0);
s16  Gp_CapTextTopY(u16* arg0);
s32  func_800E6BB8(u16* arg0);
s32  Gp_CapBusy(void);
s32  Gp_AbortCap(void);
void Gp_ResetCap(void);
/// Pointer to the loaded `.pe2cap2` blob (folder slot type 3).
extern s32 Gp_CapFile;
void       Gp_LoadCapFile(s32 arg0);
/// Key of the event the running cap script stopped on (`Gp_CapEventKey`).
s32  Gp_GetCapEventKey(void);
void Gp_RunCapCmd1(s32 arg0);
void Gp_MsgAlly3F3(s32 arg0);
void func_800E6D4C(s16 arg0, s16 arg1);
void func_800E3FAC(s32 arg0, s32 arg1);
void func_800E8614(s32 arg0, s32 arg1);
void func_800E8634(s32 arg0, s32 arg1, s32 arg2);
void Gp_AllyAnimId(s32* arg0);
void Gp_FillPlayerHpMp(void);

/// Screen-shake task. `spawnArg2` is a packed s32: low byte is the
/// duration bound (counter runs `-lo` .. `+lo`); `>> 8` is amplitude.
/// Each frame an LCG (`Gp_LcgState`) scales the remaining count into
/// `Display_ClampField126`, flipping sign on `spawnArg1` parity.
void Gp_ShakeTask(Task* arg0);
void Gp_UpdatePadInput(void);
u16  Gp_RemapButtons(GameActor* actor, u16 mask);

void func_800E9BDC(u8 arg0, s32 arg1);
void Gp_ResetMenuLock(void);
void Gp_InitState1C(Task* arg0);
void Gp_TickState1C(void);
s32  Gp_TraceGroundCoord(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1);
s32  func_800EA1A8(VECTOR3* arg0, VECTOR3* arg1);
s32  func_800EA318(s16 arg0, s16 arg1, s16 arg2);
void func_800EA3A0(s32 arg0);
void Gp_DecRoomCoordRefs(void);
void Gp_InitRoomCoords(void);
/// Spawns a `GpState1C` effect task and its `GpEffWork` (`Mem_Calloc(0x2C)`).
/// `arg0` packs the `Task_Spawn` bank in bits 16..30 and the type in the low
/// 16 bits; a negative `arg0` bypasses the 0x80 live-effect cap in
/// `GpState1C::field_0`. `arg1` is the parent coordinate (`NULL` = world):
/// the task's own `GameActorExt::field_8` coordinate is seeded from it and
/// re-parented to `Gfx_ViewCoord`. `arg2` becomes `Task::spawnArg1`; `arg3` is an
/// optional offset vector (`NULL` = zero) rotated into the parent's space and
/// kept in `GpEffWork::field_C`. Returns the work object, or `NULL`.
struct _GpEffWork* Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, SVECTOR* arg3);
/// Full-screen semi-trans POLY_F4. `arg0` is RGB; `arg1` is ABR (low 2 bits).
void Gp_DrawFadeQuad(u8* arg0, s32 arg1);
/// Handwritten GTE routine. Draws a textured sprite at `arg0`; `arg1` is a
/// signed half-extent, `arg2` a scale, and `arg3` the RGB triple.
void Gp_DrawArc(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* arg3);
/// Handwritten GTE routine. Draws an eight-segment gouraud ring centred on
/// `arg0`'s world position: `arg1` is the radius in world units (scaled by
/// 64 and divided by the projected OTZ) and `arg2` the RGB triple, which
/// only lights the ring's inner vertex so each `POLY_G4` fades to black.
void Gp_DrawRing(GsCOORDINATE2* arg0, s32 arg1, u8* arg2);
/// Draws one textured, additive `POLY_FT4` billboard at `arg0`'s projected
/// position. `arg1` is the animation frame (U origin `arg1 * 32`, the sprite
/// is 0x20 x 0x20 at V 0x18), `arg2` the radius (scaled by 31 and divided by
/// the projected OTZ) and `arg3` packs the sprite's CLUT index
/// (`Gp_QuadClutX`) in its top nibble and the spin angle in its low 12 bits,
/// so the quad's corners sit at `angle` and `angle + 0x400`.
void Gp_DrawFxQuad(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, u16 arg3);
void func_800EB6E8(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void Gp_DrawBandEx(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, u8* arg3);
void Gp_ReleaseState1CMem(void* arg0, Task* arg1);
void Gp_KillState1CTask(Task* arg0);
void Gp_PulseState1C(void);
void Gp_AddTpage(P_TAG* arg0, s32 arg1, s32 arg2);
void Gp_AddTpageShift(P_TAG* arg0, s32 arg1, s32 arg2);
void func_800EC9C8(void);
void Gp_SetState1CPe(s32 arg0);
void func_800ECA54(void);

#endif // GAMEPLAY_3CD8_H
