#include "gameplay/scene_runtime.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libcd.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/display.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/inventory.h"
#include "gameplay/items.h"
#include "items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "scene_runtime.h"
#include "gameplay/world_targets.h"
#include "world_targets.h"

#include "main/cdaudio.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/loadui.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"

/// Packed translation + rotation (no `SVECTOR` pad). `Gp_AnimBlendPose`
/// GPF/GPL-blends `vx`/`vy`/`vz` and copies `rx`/`ry`/`rz` into
/// `GpAnimScratch80.vec0` / `vec1`. `func_800B3448` dispatches here when
/// `GpAnimSlot.poseKind == 1`.
typedef struct _GpPackedPose {
    /* 0x00 */ s16 vx;
    /* 0x02 */ s16 vy;
    /* 0x04 */ s16 vz;
    /* 0x06 */ s16 rx;
    /* 0x08 */ s16 ry;
    /* 0x0A */ s16 rz;
} GpPackedPose;
STATIC_ASSERT_SIZEOF(GpPackedPose, 0xC);

/// Source/dest pointers for `Gp_AnimBlendPacked` / `Gp_AnimBlendPose`. Lives at
/// offset 4 of the 0x18-byte scratch `func_800B3448` allocates from
/// `G_SCRATCH_HEAD`. `field_0` / `field_4` are the current and next-frame
/// sources (`GpPackedSvec` when the slot's `poseKind` is 4, `GpPackedPose`
/// when it is 1); `field_8` is an optional packed dest (`arg3` of
/// `func_800B3448`). `field_C` is `arg2` of `func_800B3448` (optional
/// translation dest); `field_10` is a copy of `GpAnimSlot.bufPose`.
typedef struct _GpAnimBlendSrc {
    /* 0x00 */ GpPackedSvec* field_0;
    /* 0x04 */ GpPackedSvec* field_4;
    /* 0x08 */ GpPackedSvec* field_8;
    /* 0x0C */ GpAnimPose*   field_C;
    /* 0x10 */ u8            field_10;
} GpAnimBlendSrc;
STATIC_ASSERT_SIZEOF(GpAnimBlendSrc, 0x14);

/// 0x80-byte scratch from `G_SCRATCH_HEAD` used by `Gp_AnimBlendPacked` /
/// `Gp_AnimBlendPose` / `Gp_BlendAnimRot`. `trans` is the GPF/GPL-blended
/// translation (`Gp_AnimBlendPose`); `vec0` / `vec1` are unpacked from
/// `GpAnimBlendSrc.field_0` / `field_4`; `blend` / `invBlend` are the
/// 12-bit GPF/GPL weights. `Gp_BlendAnimRot` also uses the matrices.
typedef struct _GpAnimScratch80 {
    /* 0x00 */ SVECTOR trans;
    /* 0x08 */ SVECTOR vec0;
    /* 0x10 */ SVECTOR vec1;
    /* 0x18 */ MATRIX  mtx0;
    /* 0x38 */ MATRIX  mtx1;
    /* 0x58 */ MATRIX  mtx2;
    /* 0x78 */ s32     blend;
    /* 0x7C */ s32     invBlend;
} GpAnimScratch80;
STATIC_ASSERT_SIZEOF(GpAnimScratch80, 0x80);

/// 0x18-byte scratch `func_800B3448` allocates from `G_SCRATCH_HEAD` before
/// dispatching to `Gp_AnimBlendPose` / `Gp_AnimBlendPacked`; only `src` is
/// written.
typedef struct _GpAnimScratch18 {
    /* 0x00 */ s32            field_0;
    /* 0x04 */ GpAnimBlendSrc src;
} GpAnimScratch18;
STATIC_ASSERT_SIZEOF(GpAnimScratch18, 0x18);

/// 8-byte mask/flag record. `Gp_SndMaskTable` is a 0-terminated table of these.
/// `Gp_ApplySndMasks` / `Gp_ApplySndBankMasks` walk it: if `arg0 & mask`, apply `flags`
/// to `SndEvt_EnqueueType7` / `SndBank_SetEnableFlags`.
typedef struct _GpSndMaskRec {
    /* 0x0 */ s32 mask;
    /* 0x4 */ s32 flags;
} GpSndMaskRec;
STATIC_ASSERT_SIZEOF(GpSndMaskRec, 8);

/// 0x3C-byte stream header read before the sector payload and copied into
/// `CdCmdQueue.field_58`. The payload uses `field_30` sectors and buffer kind
/// `field_34`; `field_36` controls whether an already-loaded part is skipped.
typedef struct _GpSectorHeader {
    /* 0x00 */ u8    pad_0[0x20];
    /* 0x20 */ void* field_20;
    /* 0x24 */ u8    pad_24[0xC];
    /* 0x30 */ s16   field_30;
    /* 0x32 */ u16   field_32;
    /* 0x34 */ s16   field_34;
    /* 0x36 */ s16   field_36;
    /* 0x38 */ u8    pad_38[4];
} GpSectorHeader;
STATIC_ASSERT_SIZEOF(GpSectorHeader, 0x3C);

/// 8-byte RGB555-unpacked vector. `Gp_BlendRgb555` allocates three of
/// these (0x18 bytes) from `G_SCRATCH_HEAD`: src0, src1, then the GTE
/// lerp result. Channels are 5-bit values shifted left 7.
typedef struct _GpRgbScratch {
    /* 0x00 */ u16 r;
    /* 0x02 */ u16 g;
    /* 0x04 */ u16 b;
    /* 0x06 */ u16 pad;
} GpRgbScratch;
STATIC_ASSERT_SIZEOF(GpRgbScratch, 8);

/// 0x28-byte scratch from `G_SCRATCH_HEAD` used by `Gp_MakeDirOffset`.
/// `vec` is the `arg1->pos - arg0` delta (normalized in place);
/// `mtx` is the transpose of `gGfxViewCoord.workm`.
typedef struct _GpDirScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ MATRIX  mtx;
} GpDirScratch;
STATIC_ASSERT_SIZEOF(GpDirScratch, 0x28);

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by `Gp_DrawFloorQuad`.
/// `vec[]` holds the four corners of an axis-aligned XZ square of side
/// `size` anchored at the caller's origin; each is projected with a
/// separate RTPS. `dp` / `flag` / `otz` receive `gte_stdp` / `gte_stflg` /
/// `gte_stszotz` of the current corner, `sxy0`..`sxy3` the projected screen
/// positions copied into the `POLY_FT4`, and `maxotz` the running maximum
/// `otz` used as the OT bucket.
typedef struct _GpFloorQuadScratch {
    /* 0x00 */ SVECTOR vec[4];
    /* 0x20 */ s32     otz;
    /* 0x24 */ s32     dp;
    /* 0x28 */ s32     flag;
    /* 0x2C */ DVECTOR sxy0;
    /* 0x30 */ DVECTOR sxy1;
    /* 0x34 */ DVECTOR sxy2;
    /* 0x38 */ DVECTOR sxy3;
    /* 0x3C */ s32     maxotz;
} GpFloorQuadScratch;
STATIC_ASSERT_SIZEOF(GpFloorQuadScratch, 0x40);

u8* D_80114D10;

u16 D_80114D14[2];

s16 D_80114D18;

s16 D_80114D1A;

s16 D_80114D1C;

s32 D_80114D20;

/// 0-terminated `GpSndMaskRec` table walked by `Gp_ApplySndMasks` / `Gp_ApplySndBankMasks`.
extern GpSndMaskRec Gp_SndMaskTable[];

/// Printed when an enemy's work block cannot be allocated.
static const char Gp_StrNewEnemyNull[];

/// Three-entry dispatcher table: `Gp_EnemyWaitStart`, `Gp_EnemyWaitTick`, `Gp_DestroyEnemy`.
static const GpEnemyTaskFuncTable3 Gp_EnemyWaitFuncs;

static const TaskFuncTable3 Gp_StageLoadStates;

static const VECTOR D_80093A28;

static const TaskFuncTable3 D_80093A38;

/// "ERROR: ex_pdriver_2\n". The three bytes after the terminator are not zero:
/// the original toolchain left them in the alignment gap.
static const char D_80093A44[24];

static const TaskFuncTable3 D_80093A5C;

typedef struct {
    s32 id;
    union {
        s32 (*find)(Task*, Task*, s32, Task**);
        s32 (*exit)(Task*);
        s32 (*send)(Task*, s32, s32, s32);
    } handler;
} GpSlot4MessageEntry;

extern GpSlot4MessageEntry Gp_Slot4MsgTable[5];

s32 Gp_FindChildType9(Task* arg0, Task* arg1, s32 arg2, Task** arg3);

s32 Gp_FindChildExceptType9(Task* arg0, Task* arg1, s32 arg2, Task** arg3);

s32 Gp_ExitChildrenType9(Task* arg0);

s32 Gp_SendMsgType9(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

static void Gp_ApplySndMasks(u16 arg0);

static GpEnemy* Gp_SpawnEnemy(s32 bank, s32 type, s32 arg2, GpEnemy* parent);

static GpEnemy* Gp_AllocEnemy(Task* task, GpEnemy* parent);

static void Gp_EnemyWaitStart(GpEnemy* enemy, Task* task);

static void Gp_EnemyWaitTick(GpEnemy* enemy, Task* task);

static s32 Gp_TryEnqueueSndCd(s32 arg0);

void func_800B06F0(Task* arg0);

static void Gp_StartStageLoad(Task* task);

static void Gp_FinishStageLoad(Task* task);

static void Gp_StageLoadState2(Task* task);

static void func_800B1EFC(Task* t);

/// Unpacks two RGB555 colors, GPF/GPL-blends them by `arg2` / `0x1000 -
/// arg2`, packs the result into `*arg3`, and copies the STP bit if
/// either source has it set.
static void Gp_BlendRgb555(u16* arg0, u16* arg1, s32 arg2, u16* arg3);

static void Gp_BlendRgb555ClutMasked(u16* arg0, u16* arg1, s32 arg2, u16* arg3, s32 arg4);

static void func_800B28E0(Task* task);

static void Gp_BlendAnimRot(GpAnimBlendSrc* arg0, GpCoord* arg1, GpAnimSlot* arg2,
                            GpAnimScratch80* s);

static void Gp_AnimBlendPose(GpAnimBlendSrc* arg0, GpCoord* arg1, GpAnimSlot* arg2);

static void Gp_AnimBlendPacked(GpAnimBlendSrc* arg0, GpCoord* arg1, GpAnimSlot* arg2);

static void Gp_AnimAdvanceSlot(GpAnimCtx* arg0, s32 arg1);

static inline void _gpAnimSeekSlot(GpAnimCtx* arg0, s32 arg1, u16 arg2, s32 arg3, s32 arg4);

static void Gp_AnimSeekSlotEx(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3);

static void Gp_AnimTickSlot3(GpAnimCtx* arg0, GpAnimSlot* arg1);

static void func_800B3E74(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3);

static void func_800B3EE8(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3, s32 arg4);

static void Gp_AnimSeekSlot(GpAnimCtx* arg0, s32 arg1, s32 arg2);

static void func_800B46A4(GpAnimCtx* arg0, GpAnimSlot* arg1, u16 arg2, u16 arg3);

static void func_800B4754(GpAnimCtx* arg0, GpAnimSlot* arg1, u16 arg2, u16 arg3);

static void func_800B51F4(Task* task);

static void Gp_SetCurAreaFlag2(s32 arg0);

static GpAreaObj* Gp_GetAreaObj(GpAreaKey* arg0);

static void func_800B5A48(GpAreaKey* arg0, GpAreaObj* arg1);

static GpAreaTmdRec* Gp_GetNestedAreaObj(GpAreaKey* arg0);

static void Gp_KillSlot4Children(void);

static void func_800B6014(void);

static void func_800B6094(Task* task);

/// The 2-bit state of entry `arg0` in the current stage's `Gp_Bit2Banks` flags.
static inline s32 _gpGetCurBit2Flag(s32 arg0);

/// Finds the record in the 0xFFFF-terminated `desc` table whose id is
/// `place->field_2` and spawns that enemy at `place`.
static inline void _gpSpawnPlace(GpEnemyDesc* desc, GpBit2Rec* place);

/// Inline form of `Gp_GetRelatedQty`: the most of a related item weapon
/// `item` can hold, from bank `bank`'s table, or 0 for a non-weapon id.
static inline s32 _gpRelatedQty(s32 item, s32 bank);

/// How much of `item` the rows `scan` selects hold: the stack count for
/// stackable ids (0xA0 and up), otherwise 1 if any row carries it and 0 if not.
static inline s16 _gpScanHeldQty(McItemRec* table, McItemScan* scan, s32 item);

void Gp_BindSlot4(Task* task);

void func_800B6398(void);

void func_8017FBD8(void);

extern TaskDesc D_80115D9C[];

extern TaskDesc D_80119218[];

extern TaskDesc D_8011922C[];

extern TaskDesc D_801637C8[];

extern TaskDesc D_8017D9E8[];

extern TaskDesc D_80180DBC[];

extern TaskDesc D_801810E4[];

extern TaskDesc D_80181398[];

extern TaskDesc D_80181638[];

extern TaskDesc D_8018186C[];

extern TaskDesc D_80181B30[];

extern TaskDesc D_80181B88[];

extern TaskDesc D_80181F18[];

extern TaskDesc D_80182D0C[];

extern TaskDesc D_80182E74[];

extern TaskDesc D_80182FAC[];

extern TaskDesc D_8018384C[];

extern GpBit2List D_map_akropolis_8017A7FC[];

extern u32 D_800733FC[];

extern GpBit2List D_map_dryfield_8017A564[];

extern u32 D_800734D4[];

extern GpBit2List D_map_dryfield_full_8017A46C[];

extern GpBit2List D_map_shelter_8017A998[];

extern u32 D_8007367C[];

extern GpBit2List D_map_neo_ark_8017A6EC[];

extern u32 D_80073844[];

GpSndMaskRec Gp_SndMaskTable[7] = {
    { 1, 0 },
    { 4, 0x10000000 },
    { 8, 0x50000000 },
    { 2, 0x20000000 },
    { 16, 0x40000000 },
    { 32, -0x80000000 },
    { 0, 0 },
};
TaskDesc D_8010D1FC = { 0, 192, func_800B06F0, { NULL } };

static const char           D_80093A44[];
static const TaskFuncTable3 Gp_StageLoadStates;
static const VECTOR         D_80093A28;
static const TaskFuncTable3 D_80093A38;
static const TaskFuncTable3 D_80093A5C;

static const char Gp_StrNewEnemyNull[];

static const GpEnemyTaskFuncTable3 Gp_EnemyWaitFuncs;

s32 func_800AF590(void)
{
    GpSectorHeader header;
    CdCmdQueue*    p;

    p = &CdCmd_Queue;
    switch (D_80114D14[0]) {
        case 0:
            CdGetSector(&header, 0xF);
            D_80114D1A = 1;
            D_80114D1C = (s16)header.field_36;
            if ((D_80114D1C == 0) && ((s16)p->field_246 != 0)) {
                D_80114D1A = 0;
            }
            if (header.field_30 != 0) {
                p->field_218        = 1;
                (*(D_80114D14 + 1)) = header.field_34;
                if (D_80114D1A != 0) {
                    Mem_CopyUnaligned(&header, &p->field_58[(*(D_80114D14 + 1))], 0x3C);
                }
                switch ((*(D_80114D14 + 1))) {
                    case 0:
                        D_80114D10   = p->field_184;
                        p->field_194 = header.field_20;
                    default:
                        break;
                    case 1:
                        D_80114D10 = (u8*)D_8005C36C;
                        if (p->field_190->field_1A == 1) {
                            D_80114D10 = (u8*)D_8005C36C + 0x11000;
                        }
                        if (p->field_190->field_3 == 2) {
                            D_80114D10 += p->field_190->field_1E;
                        }
                        break;
                    case 2:
                        D_80114D10 = (u8*)D_8005C370;
                        if (p->field_190->field_1A == 2) {
                            D_80114D10 = (u8*)D_8005C370 + 0x11000;
                        }
                        if (p->field_190->field_3 == 3) {
                            D_80114D10 += p->field_190->field_1E;
                        }
                        break;
                    case 3:
                        D_80114D10 = (u8*)D_8005C374;
                        if (p->field_190->field_1A == 3) {
                            D_80114D10 = (u8*)D_8005C374 + 0x11000;
                        }
                        if (p->field_190->field_3 == 4) {
                            D_80114D10 += p->field_190->field_1E;
                        }
                        break;
                    case 4:
                        D_80114D10 = p->field_198;
                        break;
                }
                if (D_80114D1A != 0) {
                    CdGetSector(D_80114D10, 0x1F1);
                }
                D_80114D10   += 0x7C4;
                D_80114D14[0] = 1U;
                D_80114D18    = (u16)header.field_30 - 1;
            }
            break;
        case 1:
            if (D_80114D18 > 0) {
                if (D_80114D1A != 0) {
                    CdGetSector(D_80114D10, 0x200);
                }
                D_80114D10 += 0x800;
                D_80114D18  = (u16)D_80114D18 - 1;
            }
            if (D_80114D18 == 0) {
                D_80114D18 = (u16)D_80114D18 - 1;
                if (CdCmd_Queue.field_23A == 0) {
                    CdCmd_Queue.field_214 = 1;
                }
                CdCmd_Queue.field_218 = 0;
                if (D_80114D1C == 0) {
                    CdCmd_Queue.field_246 = 1;
                }
                D_80114D14[0] = 0U;
            }
            break;
    }
    return 0;
}

s16 Gp_FindStreamSlot(u16 arg0, u16 arg1, u16 arg2, u16 arg3)
{
    CdCmdQueue* p;
    StreamSlot* slot;
    u16         count;
    u16         i;
    u16         found;
    s32         temp;
    u16         streamType;
    s32         seed;

    p = &CdCmd_Queue;
    if (arg0 == 0) {
        slot  = (StreamSlot*)Fs_Streams;
        count = 0xA;
    } else {
        slot  = Stream_Slots;
        count = 0xF;
    }

    for (i = 0, found = 0; i < count; i++, slot++) {
        if (slot->field_0 == 2 && slot->field_4 != 0 && slot->field_C == arg0 && slot->field_E == arg1 &&
            slot->field_10 == arg2 && slot->field_12 == arg3) {
            found = 1;
            break;
        }
    }

    if (found == 0) {
        return -1;
    }

    Mem_Set(p->field_58, 0, 0x12C);
    p->field_190 = (CdCmd190*)slot;
    p->field_216 = 1;
    temp         = slot->field_8;
    if (temp != 0) {
        p->field_188 = temp;
    } else {
        p->field_188 = 0;
    }
    p->field_23A = 0;
    p->field_21E = 0;
    p->field_218 = 0;
    p->field_21C = 0;
    streamType   = slot->field_18;
    seed         = Gp_LcgState;
    *D_80114D14  = 0;
    p->field_238 = streamType;
    p->field_1A8 = seed;
    p->field_1AC = rand();
    Gp_LcgState  = 0;
    srand(1);
    D_80114D20 = 0xFFFF;
    return i;
}

void Gp_StepCdAudioCmd(void)
{
    s32         one;
    s32         i_s1;
    CdCmdQueue* p;
    s32         seed;
    s16         ret;
    s32         save23;
    s32         sector;

    p = &CdCmd_Queue;
    {
        s32 cmd;
        cmd = p->entries[p->readIdx].cmd;
        if (cmd == 0) {
            goto end_check;
        }
        if (cmd < 0) {
            goto end_check;
        }
        if (cmd >= 0x83) {
            goto end_check;
        }
        if (cmd < 0x81) {
            goto end_check;
        }
    }

    switch (p->step) {
        case 0:
            CdCmd_SetBusy();
            p->field_20E = 2;
            ret          = CdCmd_PollStatus(0, 0);
            if (ret != 1) {
                if (ret < 2) {
                    if (ret == 0) {
                        return;
                    }
                    break;
                }
                if (ret != 2) {
                    break;
                }
                CdFlush();
            }
            if (p->field_212 == 0) {
                p->step = p->step + 1;
                break;
            }
            p->step = 6;
            goto case6;
        case 1:
        case 2:
            p->step = p->step + 1;
            break;
        case 3: {
            CdCmd190* info;

            info         = p->field_190;
            p->field_242 = 1;
            if (info->field_3 != 0) {
                sector = info->field_4;
                if ((info->field_1C - 1) / 0x800 != 0) {
                    sector += 1 + (info->field_1C - 1) / 0x800;
                }
                Fs_ReadSectorEx(p->field_190->field_4, sector, p->field_1A4, 0);
                p->step = p->step + 1;
            } else {
                p->step = 5;
            }
            break;
        }
        case 4:
            if (Fs_CdOpStatus != 0xFF) {
                break;
            }
            ret = CdCmd_PollStatus(0, 0);
            if (ret != 1) {
                if (ret < 2) {
                    if (ret == 0) {
                        return;
                    }
                    break;
                }
                if (ret != 2) {
                    break;
                }
                CdFlush();
                p->step = 3;
                break;
            }
            p->step = p->step + 1;
            break;
        case 5: {
            CdCmd190*     info;
            s32           bits;
            u16           maskbits;
            GpSndMaskRec* entry;

            info   = p->field_190;
            sector = info->field_4;
            if (info->field_3 != 0) {
                sector += 1;
                sector += (info->field_1C - 1) / 0x800;
            }
            CdAudio_StartTrack(sector, p->field_190->field_2);
            i_s1     = 0;
            maskbits = p->field_190->field_16;
            if (Gp_SndMaskTable[0].mask != 0) {
                bits = maskbits;
                do {
                    entry = &Gp_SndMaskTable[(u16)i_s1];
                    if (bits & entry->mask) {
                        SndEvt_EnqueueType7(entry->flags, 0);
                        SndBank_SetEnableFlags(0, entry->flags);
                    }
                    i_s1++;
                } while (Gp_SndMaskTable[(u16)i_s1].mask != 0);
            }
            p->field_248 = 0;
            p->field_244 = 1;
            p->step      = p->step + 1;
            break;
        }
        case 6:
        case6: {
            s32 cmd;

            if (CdAudio_Phase.field_0 != 3) {
                break;
            }
            one          = 1;
            p->field_212 = one;
            cmd          = p->entries[p->readIdx].cmd;
            if (cmd == 0x82) {
                CdCmd_LoadActiveEntry();
                CdCmd_AdvanceRead();
                break;
            }
            if (cmd != 0x81) {
                break;
            }
            save23       = Mc_SaveData[0].demoScene;
            p->field_20E = one;
            if (save23 != 0) {
                SndEvt_EnqueueType6(0, 0, 0);
            }
            if (p->field_190->field_3 != 0) {
                p->field_240 = one;
            }
            p->field_1A0 = 0;
            CdAudio_RequestStopB();
            p->field_244 = one;
            p->field_242 = 0;
            p->step      = p->step + 1;
            break;
        }
        case 7: {
            CdCmd190*     info;
            s32           i;
            s32           bits;
            u16           maskbits;
            GpSndMaskRec* entry;

            if (CdAudio_Phase.field_1 != 4) {
                break;
            }
            if (Mc_SaveData[0].demoScene != 0) {
                SndEvt_EnqueueType6(0, 0, 0);
            }
            Mem_Set(&p->field_40, 0, 0x10);
            info            = p->field_190;
            p->field_50.cmd = 0;
            if (info->field_14 != 0) {
                CdAudio_JumpToSector(info->field_4 + info->field_14);
                p->field_242 = 1;
                p->step      = p->step + 1;
                break;
            }
            i        = 0;
            maskbits = info->field_16;
            if (Gp_SndMaskTable[0].mask != 0) {
                bits = maskbits;
                do {
                    entry = &Gp_SndMaskTable[(u16)i];
                    if (bits & entry->mask) {
                        SndBank_SetEnableFlags(1, entry->flags);
                    }
                    i++;
                } while (Gp_SndMaskTable[(u16)i].mask != 0);
            }
            {
                CdCmdQueue* q;
                s32         ff;
                q            = &CdCmd_Queue;
                seed         = q->field_1AC;
                ff           = 0xFF;
                p->field_244 = 0;
                p->field_20E = 0;
                q->field_1FE = ff;
                q->field_23A = 1;
                q->field_214 = 0;
                q->field_212 = 0;
                q->field_216 = 0;
                q->field_240 = 0;
                Gp_LcgState  = q->field_1A8;
                srand(seed);
            }
            CdCmd_AdvanceRead();
            break;
        }
        case 8: {
            s32           i;
            s32           bits;
            u16           maskbits;
            GpSndMaskRec* entry;

            if (CdAudio_Phase.field_4 != 0xA) {
                break;
            }
            i        = 0;
            maskbits = p->field_190->field_16;
            if (Gp_SndMaskTable[0].mask != 0) {
                bits = maskbits;
                do {
                    entry = &Gp_SndMaskTable[(u16)i];
                    if (bits & entry->mask) {
                        SndBank_SetEnableFlags(1, entry->flags);
                    }
                    i++;
                } while (Gp_SndMaskTable[(u16)i].mask != 0);
            }
            {
                CdCmdQueue* q;
                s32         ff;
                q            = &CdCmd_Queue;
                seed         = q->field_1AC;
                ff           = 0xFF;
                p->field_242 = 0;
                p->field_244 = 0;
                p->field_20E = 0;
                q->field_1FE = ff;
                q->field_23A = 1;
                q->field_214 = 0;
                q->field_212 = 0;
                q->field_216 = 0;
                q->field_240 = 0;
                Gp_LcgState  = q->field_1A8;
                srand(seed);
            }
            CdCmd_AdvanceRead();
            break;
        }
    }

end_check:
    CdCmd_StepVlcRebuild();
}

static void Gp_ApplySndMasks(u16 arg0)
{
    s32           i;
    s32           bits;
    GpSndMaskRec* entry;

    i = 0;
    if (Gp_SndMaskTable[0].mask != 0) {
        bits = arg0;
        do {
            entry = &Gp_SndMaskTable[(u16)i];
            if (bits & entry->mask) {
                SndEvt_EnqueueType7(entry->flags, 0);
                SndBank_SetEnableFlags(0, entry->flags);
            }
            i++;
        } while (Gp_SndMaskTable[(u16)i].mask != 0);
    }
}

void Gp_ApplySndBankMasks(u16 arg0)
{
    s32           i;
    s32           bits;
    GpSndMaskRec* entry;

    i = 0;
    if (Gp_SndMaskTable[0].mask != 0) {
        bits = arg0;
        do {
            entry = &Gp_SndMaskTable[(u16)i];
            if (bits & entry->mask) {
                SndBank_SetEnableFlags(1, entry->flags);
            }
            i++;
        } while (Gp_SndMaskTable[(u16)i].mask != 0);
    }
}

void Gp_RestoreStreamRng(void)
{
    CdCmdQueue* p;

    p            = &CdCmd_Queue;
    p->field_1FE = 0xFF;
    p->field_23A = 1;
    p->field_214 = 0;
    p->field_212 = 0;
    p->field_216 = 0;
    p->field_240 = 0;
    Gp_LcgState  = p->field_1A8;
    srand(p->field_1AC);
}

s32 func_800B0118(s32 arg0, s32 arg1)
{
    s16 temp;

    temp = arg0;
    if (temp != 0) {
        D_80114D20          = temp;
        GameMain_HaltFlags |= 8;
    } else {
        GameMain_HaltFlags &= ~8;
    }
    return 0;
}

void Gp_SetStreamBuf(void* arg0)
{
    CdCmd_Queue.field_198 = arg0;
}

static GpEnemy* Gp_SpawnEnemy(s32 bank, s32 type, s32 arg2, GpEnemy* parent)
{
    Task*    task;
    GpEnemy* ret;

    task = Task_Spawn(bank, type, arg2, 0);
    if (task != NULL) {
        ret = Gp_AllocEnemy(task, parent);
    } else {
        ret = NULL;
    }
    return ret;
}

GpEnemy* Gp_SpawnEnemyFromTable(TaskDesc* table, s32 idx, s32 arg2, GpEnemy* parent)
{
    Task*    task;
    GpEnemy* ret;

    task = Task_SpawnFromTable(table, idx, arg2, 0);
    if (task != NULL) {
        ret = Gp_AllocEnemy(task, parent);
    } else {
        ret = NULL;
    }
    return ret;
}

void Gp_DestroyEnemy(GpEnemy* enemy, Task* task)
{
    Gp_UnlinkNode(&enemy->node);
    memFree(enemy);
    taskKill(task);
}

void Gp_EnemyTaskExit(Task* task)
{
    GpEnemy* enemy;

    enemy = task->spawnArg2;
    Gp_UnlinkNode(&enemy->node);
    memFree(enemy);
    taskKill(task);
}

Task* Gp_CopyCoordOffset(Task* arg0, GpCoord* arg1, SVECTOR* arg2)
{
    TmdObject* extra;
    GpCoord*   dest;
    GpCoord*   world;

    if (arg0 == NULL) {
        return NULL;
    }

    SCRATCH_PUSH_BYTES(8);
    world = &gGfxViewCoord;
    extra = arg0->extra.tmd;
    dest  = extra->coords;
    if (arg1->sub == world) {
        dest->coord = arg1->coord;
        gte_SetRotMatrix(&arg1->coord);
        gte_SetTransMatrix(&arg1->coord);
        gte_ldv0(arg2);
        gte_rtv0tr();
        gte_stlvnl(dest->coord.t);
    } else {
        Gp_UpdateCoord(arg1);
        dest->workm = arg1->workm;
        gte_SetRotMatrix(&arg1->workm);
        gte_SetTransMatrix(&arg1->workm);
        gte_ldv0(arg2);
        gte_rtv0tr();
        gte_stlvnl(dest->workm.t);
        Gp_WorldToLocal(&world->workm, &dest->workm, &dest->coord);
    }
    dest->sub = &gGfxViewCoord;
    dest->flg = 0;
    SCRATCH_POP_BYTES(8);
    return arg0;
}

static GpEnemy* Gp_AllocEnemy(Task* task, GpEnemy* parent)
{
    GpEnemy* enemy;

    enemy = memCalloc(0x60, 0);
    if (enemy == NULL) {
        printf(Gp_StrNewEnemyNull);
        taskKill(task);
        return NULL;
    }

    task->exitCallback = Gp_EnemyTaskExit;
    task->spawnArg2    = enemy;
    enemy->task        = task;
    enemy->coord       = &gGfxViewCoord;
    if (parent != NULL) {
        Task_Reparent(parent->task, task);
    } else {
        Task_Reparent(gameGetPtrSlot(4), enemy->task);
    }
    return enemy;
}

static void Gp_EnemyWaitStart(GpEnemy* enemy, Task* task)
{
    enemy->waitTicks = 0x78;
    task->state++;
}

static void Gp_EnemyWaitTick(GpEnemy* enemy, Task* task)
{
    enemy->waitTicks--;
    if (enemy->waitTicks == 0) {
        task->state++;
    }
}

void Gp_EnemyDispatch(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Gp_EnemyWaitFuncs;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

static s32 Gp_TryEnqueueSndCd(s32 arg0)
{
    u8 param1[8];
    u8 param2[8];

    if (CdCmd_IsIdle() & 0xFFFF) {
        param1[0] = arg0;
        param1[3] = 0;
        param1[2] = 5;
        param2[0] = 1;
        param2[1] = 1;
        param2[3] = 0;
        param2[2] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        D_800626E8 = 1;
        return 0;
    }
    return 0xFF;
}

void Gp_EnqueueSndCd(u8 arg0)
{
    u8  param1[8];
    u8  param2[8];
    s32 flag;

    if (gGameSession->loadedSndId != arg0) {
        SndEvt_EnqueueType7(0xE0000000, 8);
        flag      = 1;
        param1[3] = 0;
        param1[2] = 5;
        param1[0] = arg0;
        param2[0] = flag;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        D_800626E8                = flag;
        gGameSession->loadedSndId = arg0;
    }
}

void func_800B06F0(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = Gp_StageLoadStates;
    sp.funcs[arg0->state](arg0);
}

static void Gp_StartStageLoad(Task* task)
{
    s32           i;
    u8            param1[8];
    u8            param2[8];
    FsFolderSlot* table;
    s32           fileId;

    if (Midi_IsBusy(0) == 0) {
        gDisplayState.loadBusy = 1;
        i                      = 0;
        table                  = D_8006C338;
        do {
            table[(u8)i].field_0 = 0;
            i++;
        } while ((u8)i < 0x32);

        fileId = 0xA;
        if (gGameSession->restartMode != 0xFF) {
            param1[2] = 4;
            param1[0] = 0x62;
            param1[3] = 0;
            param2[0] = 1;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
            fileId = 9;
        }
        CdCmd_EnqueueLoadFile(fileId, 0, 3);
        gDisplayState.skipDraw = 0;
        task->state++;
    }
}

static void Gp_FinishStageLoad(Task* task)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        gDisplayState.at100.flags.imageSource = 1;
        if (gGameSession->restartMode == 0xFF) {
            Task_SpawnFromTable(D_8011922C, 0, 0, 0);
            taskKill(task);
        } else {
            task->spawnArg2 = Task_SpawnFromTable(D_80115D9C, 0, 0, 0);
            SndEvt_EnqueueType1(0x62, 0);
        }
        task->state++;
    }
}

static void Gp_StageLoadState2(Task* task)
{
    s32           out;
    DisplayState* ds;

    if (Task_PollKill(task->spawnArg2, &out) != 0) {
        ds                  = &gDisplayState;
        task->killCountdown = 0;
        ds->gameMode        = 1;
        ds->loadBusy        = 0;
        taskKill(task);
    }
}

void func_800B0928(Task* arg0, Task* arg1, s32 arg2, s32 arg3, s32 arg4)
{
    VECTOR   tmp;
    VECTOR   acc0;
    VECTOR   acc1;
    SVECTOR  delta;
    SVECTOR  ang;
    SVECTOR  euler;
    MATRIX   mtx0;
    MATRIX   mtx1;
    MATRIX   tmtx;
    s32      i;
    GpCoord* rec;
    GpCoord* rec1;
    s32      pitchLimit;
    s32      yawLimit;
    s32      pitchMagnitude;
    s32      yawMagnitude;
    MATRIX*  m0;
    MATRIX*  m1;
    GpCoord* base;
    MATRIX*  m;

    i                        = 0;
    m0                       = &mtx0;
    *(s32*)&mtx0             = ONE;
    MATRIX_PAIR(&mtx0, 0, 2) = 0;
    MATRIX_PAIR(m0, 1, 1)    = ONE;
    MATRIX_PAIR(&mtx0, 2, 0) = 0;
    m0->m[2][2]              = ONE;
    acc0.vx                  = 0;
    acc0.vy                  = 0;
    acc0.vz                  = 0;
    for (i = 0; i < 4; i++) {
        rec = &arg0->extra.tmd->coords[i];
        ApplyMatrixLV(&mtx0, (VECTOR*)rec->coord.t, &tmp);
        acc0.vx += tmp.vx;
        acc0.vy += tmp.vy;
        acc0.vz += tmp.vz;
        MulMatrix0(&rec->coord, &mtx0, &mtx0);
    }
    rec = &arg0->extra.tmd->coords[i];
    ApplyMatrixLV(&mtx0, (VECTOR*)rec->coord.t, &tmp);
    i                        = 0;
    m1                       = &mtx1;
    *(s32*)&mtx1             = ONE;
    MATRIX_PAIR(&mtx1, 0, 2) = 0;
    MATRIX_PAIR(m1, 1, 1)    = ONE;
    MATRIX_PAIR(&mtx1, 2, 0) = 0;
    m1->m[2][2]              = ONE;
    acc1.vx                  = 0;
    acc1.vy                  = 0;
    acc1.vz                  = 0;
    for (i = 0; i < 4; i++) {
        rec1 = &arg1->extra.tmd->coords[i];
        ApplyMatrixLV(&mtx1, (VECTOR*)rec1->coord.t, &tmp);
        acc1.vx += tmp.vx;
        acc1.vy += tmp.vy;
        acc1.vz += tmp.vz;
        MulMatrix0(&rec1->coord, &mtx1, &mtx1);
    }
    rec1 = &arg1->extra.tmd->coords[i];
    ApplyMatrixLV(&mtx1, (VECTOR*)rec1->coord.t, &tmp);

    delta.vx = (u16)acc1.vx - (u16)acc0.vx;
    delta.vy = (u16)acc1.vy - (u16)acc0.vy;
    delta.vz = (u16)acc1.vz - (u16)acc0.vz;
    TransposeMatrix(&mtx0, &tmtx);
    ApplyMatrix(&tmtx, &delta, &acc0);

    ang.vx = ratan2(-acc0.vy, acc0.vz >= 0 ? acc0.vz : -acc0.vz);
    ang.vy = ratan2(acc0.vx, acc0.vz);
    ang.vz = 0;

    base = arg0->extra.tmd->coords;
    rec  = base + 4;
    Gp_MtxToEuler(&base[4].coord, &euler);

    ang.vx     = euler.vx + (ang.vx - euler.vx) * arg4 / 4096;
    ang.vy     = euler.vy + (ang.vy - euler.vy) * arg4 / 4096;
    ang.vz     = euler.vz;
    pitchLimit = euler.vx >= 0 ? euler.vx : -euler.vx;
    if (arg3 < pitchLimit) {
        arg3 = pitchLimit;
    }
    yawLimit = euler.vy >= 0 ? euler.vy : -euler.vy;
    if (arg2 < yawLimit) {
        arg2 = yawLimit;
    }
    pitchMagnitude = ang.vx >= 0 ? ang.vx : -ang.vx;
    if (arg3 < pitchMagnitude) {
        ang.vx = ang.vx < 0 ? -arg3 : arg3;
    }
    yawMagnitude = ang.vy >= 0 ? ang.vy : -ang.vy;
    if (arg2 < yawMagnitude) {
        ang.vy = ang.vy < 0 ? -arg2 : arg2;
    }

    m                    = &rec->coord;
    *(s32*)&rec->coord   = ONE;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 1, 1) = ONE;
    MATRIX_PAIR(m, 2, 0) = 0;
    m->m[2][2]           = ONE;
    RotMatrix(&ang, m);
    rec->flg = 0;
}

void func_800B0CF4(Task* arg0, GpCoord* arg1, s32 arg2, s32 arg3, s32 arg4)
{
    VECTOR   transformed;
    VECTOR   position;
    VECTOR   target;
    SVECTOR  offset;
    SVECTOR  angles;
    SVECTOR  current;
    MATRIX   world;
    MATRIX   inverse;
    MATRIX*  mtx;
    MATRIX*  outMtx;
    GpCoord* part;
    s32      i;
    s32      pitchMagnitude;
    s32      yawMagnitude;
    s32      pitchLimit;
    s32      yawLimit;

    mtx                       = &world;
    MATRIX_PAIR(&world, 0, 0) = 0x1000;
    MATRIX_PAIR(&world, 0, 2) = 0;
    MATRIX_PAIR(mtx, 1, 1)    = 0x1000;
    MATRIX_PAIR(&world, 2, 0) = 0;
    mtx->m[2][2]              = 0x1000;
    position.vx               = 0;
    position.vy               = 0;
    position.vz               = 0;
    for (i = 0; i < 5; i++) {
        part = &arg0->extra.tmd->coords[i];
        ApplyMatrixLV(&world, (VECTOR*)part->coord.t, &transformed);
        position.vx += transformed.vx;
        position.vy += transformed.vy;
        position.vz += transformed.vz;
        MulMatrix0(&world, &part->coord, &world);
    }
    target.vx = arg1->coord.t[0];
    target.vy = arg1->coord.t[1];
    target.vz = arg1->coord.t[2];
    offset.vx = target.vx - position.vx;
    offset.vy = target.vy - position.vy;
    offset.vz = target.vz - position.vz;
    TransposeMatrix(&world, &inverse);
    ApplyMatrix(&inverse, &offset, &position);
    angles.vx = -ratan2(position.vy, position.vz);
    angles.vy = ratan2(position.vx, position.vz);
    angles.vz = 0;
    part      = &arg0->extra.tmd->coords[4];
    Gp_MtxToEuler(&part->coord, &current);
    angles.vx  = current.vx + (angles.vx - current.vx) * arg4 / 4096;
    angles.vy  = current.vy + (angles.vy - current.vy) * arg4 / 4096;
    angles.vz  = current.vz;
    pitchLimit = current.vx >= 0 ? current.vx : -current.vx;
    if (arg3 < pitchLimit) {
        arg3 = pitchLimit;
    }
    yawLimit = current.vy >= 0 ? current.vy : -current.vy;
    if (arg2 < yawLimit) {
        arg2 = yawLimit;
    }
    pitchMagnitude = angles.vx >= 0 ? angles.vx : -angles.vx;
    if (arg3 < pitchMagnitude) {
        angles.vx = angles.vx < 0 ? -arg3 : arg3;
    }
    yawMagnitude = angles.vy >= 0 ? angles.vy : -angles.vy;
    if (arg2 < yawMagnitude) {
        angles.vy = angles.vy < 0 ? -arg2 : arg2;
    }
    outMtx                          = &part->coord;
    MATRIX_PAIR(&part->coord, 0, 0) = 0x1000;
    MATRIX_PAIR(outMtx, 0, 2)       = 0;
    MATRIX_PAIR(outMtx, 1, 1)       = 0x1000;
    MATRIX_PAIR(outMtx, 2, 0)       = 0;
    outMtx->m[2][2]                 = 0x1000;
    RotMatrix(&angles, outMtx);
}

void Gp_MtxToEuler(MATRIX* arg0, SVECTOR* arg1)
{
    SVECTOR in;
    SVECTOR out;
    MATRIX  mtx;
    s32     one;
    s16     len;

    mtx      = *arg0;
    one      = 0x1000;
    mtx.t[2] = 0;
    mtx.t[1] = 0;
    mtx.t[0] = 0;
    in.vx    = 0;
    in.vy    = 0;
    in.vz    = one;
    ApplyMatrixSV(&mtx, &in, &out);
    arg1->vx = -ratan2(out.vy, out.vz);
    len      = SquareRoot12((out.vz * out.vz + out.vy * out.vy) >> 12);
    arg1->vy = ratan2(out.vx, len);
    in.vx    = 0;
    in.vy    = one;
    in.vz    = 0;
    ApplyMatrixSV(&mtx, &in, &out);
    in.vx = -arg1->vx;
    in.vy = -arg1->vy;
    in.vz = 0;
    RotMatrixZYX(&in, &mtx);
    ApplyMatrixSV(&mtx, &out, &in);
    arg1->vz = -ratan2(in.vx, in.vy);
}

SVECTOR* Gp_ExtractEuler(SVECTOR* arg0, MATRIX* arg1)
{
    SVECTOR ang0;
    SVECTOR ang1;
    s32     sin0;
    s32     cos0;
    s32     sin1;
    s32     cos1;

    ang0.vx = -ratan2(arg1->m[1][2], arg1->m[2][2]);
    ang1.vx = (ang0.vx <= 0) ? ang0.vx + 0x800 : ang0.vx - 0x800;

    sin0 = rsin(ang0.vx);
    cos0 = rcos(ang0.vx);
    sin1 = rsin(ang1.vx);
    cos1 = rcos(ang1.vx);

    ang0.vy = ratan2(arg1->m[0][2], (arg1->m[2][2] * cos0) / 4096 - (arg1->m[1][2] * sin0) / 4096);
    ang1.vy = ratan2(arg1->m[0][2], (arg1->m[2][2] * cos1) / 4096 - (arg1->m[1][2] * sin1) / 4096);

    ang0.vz = ratan2((arg1->m[1][0] * cos0) / 4096 + (arg1->m[2][0] * sin0) / 4096,
                     (arg1->m[1][1] * cos0) / 4096 + (arg1->m[2][1] * sin0) / 4096);
    ang1.vz = ratan2((arg1->m[1][0] * cos1) / 4096 + (arg1->m[2][0] * sin1) / 4096,
                     (arg1->m[1][1] * cos1) / 4096 + (arg1->m[2][1] * sin1) / 4096);

    sin0 = ABS(ang0.vx) + ABS(ang0.vy) + ABS(ang0.vz);
    cos0 = ABS(ang1.vx) + ABS(ang1.vy) + ABS(ang1.vz);
    if (sin0 < cos0) {
        *arg0 = ang0;
    } else {
        *arg0 = ang1;
    }
    return arg0;
}

void Gp_LerpOrthonormal(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2, s32 arg3)
{
    MATRIX mtx;
    MATRIX diffs;
    VECTOR vec[3];
    VECTOR tmp;
    VECTOR nrm;
    s32    i;
    s32    best;
    s32    len;
    s32    ret;

    best = 0;
    for (i = 0; i < 3; i++) {
        diffs.m[i][0] = arg1->m[i][0] - arg0->m[i][0];
        diffs.m[i][1] = arg1->m[i][1] - arg0->m[i][1];
        diffs.m[i][2] = arg1->m[i][2] - arg0->m[i][2];
    }
    for (i = 0; i < 3; i++) {
        vec[i].vx = arg0->m[i][0] + (diffs.m[i][0] * arg3) / ONE;
        vec[i].vy = arg0->m[i][1] + (diffs.m[i][1] * arg3) / ONE;
        vec[i].vz = arg0->m[i][2] + (diffs.m[i][2] * arg3) / ONE;
    }

    len = -1;

    gte_ldopv1(&vec[0]);
    gte_ldopv2(&vec[1]);
    gte_op12();
    gte_stlvnl(&tmp);
    ret = VectorNormal(&tmp, &nrm);
    if (len < ret) {
        len  = ret;
        best = 2;
    }

    gte_ldopv1(&vec[1]);
    gte_ldopv2(&vec[2]);
    gte_op12();
    gte_stlvnl(&tmp);
    ret = VectorNormal(&tmp, &nrm);
    if (len < ret) {
        len  = ret;
        best = 0;
    }

    gte_ldopv1(&vec[0]);
    gte_ldopv2(&vec[2]);
    gte_op12();
    gte_stlvnl(&tmp);
    if (len < VectorNormal(&tmp, &nrm)) {
        best = 1;
    }

    switch (best) {
        case 0:
            mtx.m[1][0] = vec[1].vx;
            mtx.m[1][1] = vec[1].vy;
            mtx.m[1][2] = vec[1].vz;
            mtx.m[2][0] = vec[2].vx;
            mtx.m[2][1] = vec[2].vy;
            mtx.m[2][2] = vec[2].vz;
            MatrixNormal_1(&mtx, arg2);
            break;
        case 1:
            mtx.m[0][0] = vec[0].vx;
            mtx.m[0][1] = vec[0].vy;
            mtx.m[0][2] = vec[0].vz;
            mtx.m[2][0] = vec[2].vx;
            mtx.m[2][1] = vec[2].vy;
            mtx.m[2][2] = vec[2].vz;
            MatrixNormal_2(&mtx, arg2);
            break;
        case 2:
            mtx.m[0][0] = vec[0].vx;
            mtx.m[0][1] = vec[0].vy;
            mtx.m[0][2] = vec[0].vz;
            mtx.m[1][0] = vec[1].vx;
            mtx.m[1][1] = vec[1].vy;
            mtx.m[1][2] = vec[1].vz;
            MatrixNormal_0(&mtx, arg2);
            break;
    }
}

void func_800B17D4(Task* arg0, Task* arg1, GpHeadAim* arg2)
{
    VECTOR   tmp;
    VECTOR   acc0;
    VECTOR   acc1;
    SVECTOR  delta;
    SVECTOR  ang;
    SVECTOR  euler;
    MATRIX   mtx0;
    MATRIX   mtx1;
    MATRIX   tmtx;
    VECTOR   probe;
    s32      rate;
    s32      inited;
    s32      i;
    GpCoord* rec;
    GpCoord* rec1;
    s32      pitchLimit;
    s32      yawLimit;
    s32      curPitch;
    s32      curYaw;
    s32      newPitch;
    s32      newYaw;
    MATRIX*  m0;
    MATRIX*  m1;
    GpCoord* base;
    MATRIX*  m;

    i          = 0;
    m0         = &mtx0;
    probe      = D_80093A28;
    yawLimit   = arg2->yawLimit;
    pitchLimit = arg2->pitchLimit;
    rate       = arg2->rate;
    inited     = arg2->inited;

    *(s32*)&mtx0             = ONE;
    MATRIX_PAIR(&mtx0, 0, 2) = 0;
    MATRIX_PAIR(m0, 1, 1)    = ONE;
    MATRIX_PAIR(&mtx0, 2, 0) = 0;
    m0->m[2][2]              = ONE;
    acc0.vx                  = 0;
    acc0.vy                  = 0;
    acc0.vz                  = 0;
    for (i = 0; i < 5; i++) {
        rec = &arg0->extra.tmd->coords[i];
        ApplyMatrixLV(&mtx0, (VECTOR*)rec->coord.t, &tmp);
        acc0.vx += tmp.vx;
        acc0.vy += tmp.vy;
        acc0.vz += tmp.vz;
        MulMatrix0(&mtx0, &rec->coord, &mtx0);
    }
    ApplyMatrixLV(&mtx0, &probe, &tmp);
    acc0.vx += tmp.vx;
    acc0.vy += tmp.vy;
    acc0.vz += tmp.vz;

    i                        = 0;
    m1                       = &mtx1;
    *(s32*)&mtx1             = ONE;
    MATRIX_PAIR(&mtx1, 0, 2) = 0;
    MATRIX_PAIR(m1, 1, 1)    = ONE;
    MATRIX_PAIR(&mtx1, 2, 0) = 0;
    m1->m[2][2]              = ONE;
    acc1.vx                  = 0;
    acc1.vy                  = 0;
    acc1.vz                  = 0;
    for (i = 0; i < 5; i++) {
        rec1 = &arg1->extra.tmd->coords[i];
        ApplyMatrixLV(&mtx1, (VECTOR*)rec1->coord.t, &tmp);
        acc1.vx += tmp.vx;
        acc1.vy += tmp.vy;
        acc1.vz += tmp.vz;
        MulMatrix0(&mtx1, &rec1->coord, &mtx1);
    }
    ApplyMatrixLV(&mtx1, &probe, &tmp);
    acc1.vx += tmp.vx;
    acc1.vy += tmp.vy;
    acc1.vz += tmp.vz;

    delta.vx = (u16)acc1.vx - (u16)acc0.vx;
    delta.vy = (u16)acc1.vy - (u16)acc0.vy;
    delta.vz = (u16)acc1.vz - (u16)acc0.vz;
    TransposeMatrix(&mtx0, &tmtx);
    ApplyMatrix(&tmtx, &delta, &acc0);

    ang.vx = ratan2(-acc0.vy, acc0.vz);
    ang.vy = ratan2(acc0.vx, acc0.vz);
    ang.vz = 0;
    if (delta.vy < 0) {
        if (ang.vx < -0x400) {
            ang.vx = (u16)ang.vx + 0x1000;
        }
    } else if (ang.vx >= 0x400) {
        ang.vx = (u16)ang.vx - 0x1000;
    }

    if (inited != 0) {
        if (ABS(ang.vx - arg2->lastPitch) > 0x800) {
            while (ang.vx >= 0x800) {
                ang.vx -= 0x1000;
            }
            while (ang.vx < -0x800) {
                ang.vx += 0x1000;
            }
        }
    } else {
        arg2->inited = 1;
    }
    arg2->lastPitch = ang.vx;

    base = arg0->extra.tmd->coords;
    rec  = base + 4;
    Gp_ExtractEuler(&euler, &base[4].coord);

    ang.vx   = euler.vx + (ang.vx - euler.vx) * rate / 4096;
    ang.vy   = euler.vy + (ang.vy - euler.vy) * rate / 4096;
    ang.vz   = euler.vz;
    curPitch = euler.vx >= 0 ? euler.vx : -euler.vx;
    if (pitchLimit < curPitch) {
        pitchLimit = curPitch;
    }
    curYaw = euler.vy >= 0 ? euler.vy : -euler.vy;
    if (yawLimit < curYaw) {
        yawLimit = curYaw;
    }
    newPitch = ang.vx >= 0 ? ang.vx : -ang.vx;
    if (pitchLimit < newPitch) {
        ang.vx = ang.vx < 0 ? -pitchLimit : pitchLimit;
    }
    newYaw = ang.vy >= 0 ? ang.vy : -ang.vy;
    if (yawLimit < newYaw) {
        ang.vy = ang.vy < 0 ? -yawLimit : yawLimit;
    }

    m                    = &rec->coord;
    *(s32*)&rec->coord   = ONE;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 1, 1) = ONE;
    MATRIX_PAIR(m, 2, 0) = 0;
    m->m[2][2]           = ONE;
    RotMatrix(&ang, m);
    rec->flg = 0;
}

void Gp_ComposeParentWorld(GpCoord* arg0, MATRIX* arg1, SVECTOR* arg2)
{
    SVECTOR tmp;
    MATRIX* m;
    s32     one;

    if (arg0->sub != &gGfxViewCoord) {
        Gp_ComposeParentWorld(arg0->sub, arg1, arg2);
    } else {
        one                  = ONE;
        m                    = arg1;
        *(s32*)m             = one;
        MATRIX_PAIR(m, 0, 2) = 0;
        MATRIX_PAIR(m, 1, 1) = one;
        MATRIX_PAIR(m, 2, 0) = 0;
        m->m[2][2]           = one;
        arg2->vx             = 0;
        arg2->vy             = 0;
        arg2->vz             = 0;
    }

    tmp.vx = (u16)arg0->coord.t[0];
    tmp.vy = (u16)arg0->coord.t[1];
    tmp.vz = (u16)arg0->coord.t[2];
    gte_SetRotMatrix(arg1);
    gte_ldv0(&tmp);
    gte_rtv0();
    gte_stsv(&tmp);
    arg2->vx += tmp.vx;
    arg2->vy += tmp.vy;
    arg2->vz += tmp.vz;
    gte_ldclmv(&arg0->coord);
    gte_rtir();
    gte_stclmv(arg1);
    gte_ldclmv(&arg0->coord.m[0][1]);
    gte_rtir();
    gte_stclmv(&arg1->m[0][1]);
    gte_ldclmv(&arg0->coord.m[0][2]);
    gte_rtir();
    gte_stclmv(&arg1->m[0][2]);
}

static void func_800B1EFC(Task* t)
{
    TILE*     p;
    DR_TPAGE* dr;
    u8        color;

    if (t->spawnArg1 > 0) {
        if (t->killCountdown > 0) {
            t->killCountdown--;
            color = ~(t->killCountdown << 3);
        } else {
            t->spawnArg1--;
            color = 0xFF;
        }
    } else {
        t->killCountdown++;
        color = ~(t->killCountdown << 3);
        if (t->killCountdown >= 0x1F) {
            t->state++;
        }
    }

    p              = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setlen(p, 3);
    setcode(p, 0x62);
    setXY0(p, -0xA0, -0x78);
    p->y0 -= gDisplayState.vramYOffset;
    p->b0  = color;
    p->g0  = color;
    p->r0  = color;
    setWH(p, 0x140, 0xF0);

    dr             = (DR_TPAGE*)gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    if (t->spawnArg2 == 0) {
        setlen(dr, 1);
        dr->code[0] = 0xE1000240;
    } else {
        setlen(dr, 1);
        dr->code[0] = 0xE1000220;
    }
    addPrim(gGpuCurrentOt, p);
    addPrim(gGpuCurrentOt, dr);
}

/// Unpacks two RGB555 colors, GPF/GPL-blends them by `arg2` / `0x1000 -
/// arg2`, packs the result into `*arg3`, and copies the STP bit if
/// either source has it set.
static void Gp_BlendRgb555(u16* arg0, u16* arg1, s32 arg2, u16* arg3)
{
    u8*           head;
    GpRgbScratch* c0;
    GpRgbScratch* c1;
    GpRgbScratch* out;
    u16           color;
    u16           packed;

    head                       = SCRATCH_HEAD(u8);
    c0                         = (GpRgbScratch*)(head - 0x18);
    SCRATCH_HEAD(GpRgbScratch) = c0;

    color = *arg0;
    c0->b = color;
    c0->g = color;
    c0->r = (color & 0x1F) << 7;
    c0->g = (c0->g << 2) & 0xF80;
    c0->b = (c0->b >> 3) & 0xF80;

    c1    = (GpRgbScratch*)(head - 0x10);
    color = *arg1;
    c1->b = color;
    c1->g = color;
    c1->r = (color & 0x1F) << 7;
    c1->g = (c1->g << 2) & 0xF80;
    c1->b = (c1->b >> 3) & 0xF80;

    gte_lddp(arg2);
    gte_ldsv(c0);
    gte_gpf12();
    gte_lddp(0x1000 - arg2);
    gte_ldsv(c1);
    gte_gpl12();
    out = (GpRgbScratch*)(head - 8);
    gte_stsv(out);

    packed = ((out->b >> 2) & 0x3E0) | ((out->g >> 7) & 0x1F);
    packed = (packed << 5) | ((out->r >> 7) & 0x1F);
    *arg3  = packed;
    if ((s16)*arg0 < 0 || (s16)*arg1 < 0) {
        *arg3 = packed | 0x8000;
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Full-screen fade quad. Ramps a 0x140x0xF0 `TILE` from black to
/// `field_2`-scaled white over `field_2` frames, holds until the owner
/// raises `field_1`, then ramps back down and kills the task. Sorted into
/// `gGpuCurrentOt[Task::spawnArg1]`, or (`spawnArg1 == 0`) into the head
/// of the current ordering table, backing up 0xA entries when the current
/// OT is not one of the two `Gpu_OrderingTables` roots.
void Gp_FadeWorkTask(Task* t)
{
    GpFadeWork* work;
    TILE*       tile;
    DR_TPAGE*   dr;
    s32         color;
    s16         y;
    s8          yoff;

    work = t->spawnArg2;

    if (t->state == 0) {
        t->killCountdown = 0;
        if (work->field_2 <= 0) {
            work->field_2 = 0x20;
        }
        t->state = t->state + 1;
    }
    if ((t->state == 2) && (work->field_1 == 1)) {
        t->killCountdown = work->field_2;
    }

    color          = (t->killCountdown * 0xFF0) / work->field_2;
    tile           = (TILE*)gGpuPrimCursor;
    y              = -0x78;
    tile->y0       = y;
    gGpuPrimCursor = tile + 1;
    setlen(tile, 3);
    setcode(tile, 0x62);
    tile->x0 = -0xA0;
    yoff     = gDisplayState.vramYOffset;
    tile->w  = 0x140;
    tile->h  = 0xF0;
    color    = color >> 4;
    tile->b0 = color;
    tile->g0 = color;
    tile->r0 = color;
    dr       = (DR_TPAGE*)gGpuPrimCursor;
    tile->y0 = y - yoff;

    gGpuPrimCursor = dr + 1;
    if (work->field_0 == 0) {
        setlen(dr, 1);
        dr->code[0] = 0xE1000240;
    } else {
        setlen(dr, 1);
        dr->code[0] = 0xE1000220;
    }

    if (t->spawnArg1 != 0) {
        u_long* ot;

        ot = gGpuCurrentOt;
        addPrim(&ot[t->spawnArg1], tile);
        addPrim(&ot[t->spawnArg1], dr);
    } else {
        u_long* ot;

        ot = gGpuCurrentOt;
        if ((ot == (u_long*)Gpu_OrderingTables[0].org) || (ot == (u_long*)Gpu_OrderingTables[1].org)) {
            addPrim(ot, tile);
            addPrim(ot, dr);
        } else {
            addPrim(&ot[-0xA], tile);
            addPrim(&ot[-0xA], dr);
        }
    }

    switch (t->state) {
        case 1:
            t->killCountdown = t->killCountdown + 1;
            if (t->killCountdown == work->field_2) {
                t->state = t->state + 1;
            }
            break;
        case 2:
            if (work->field_1 == 1) {
                t->state = 3;
            }
            break;
        case 3:
            t->killCountdown = t->killCountdown - 1;
            if (t->killCountdown <= 0) {
                work->field_1 = 2;
                taskKill(t);
            }
            break;
        default:
            taskKill(t);
            break;
    }
}

void func_800B25B0(void)
{
    switch (GP_LOC_WORD(Mc_SaveData[0].at4.loc) & GP_LOC_STAGE_AREA) {
        case GP_LOC_KEY(5, 27, 0, 0):
            Task_SpawnFromTable(D_80181F18, 0, 0, 0);
            break;
        case GP_LOC_KEY(5, 15, 0, 0):
            Task_SpawnFromTable(D_80181398, 0, 0, 0);
            break;
        case GP_LOC_KEY(5, 14, 0, 0):
            Task_SpawnFromTable(D_80181B30, 0, 0, 0);
            break;
        case GP_LOC_KEY(5, 13, 0, 0):
            Task_SpawnFromTable(D_8018384C, 0, 0, 0);
            break;
        case GP_LOC_KEY(5, 12, 0, 0):
            Task_SpawnFromTable(D_801810E4, 1, 0, 0);
            break;
        case GP_LOC_KEY(5, 7, 0, 0):
            Task_SpawnFromTable(D_80180DBC, 0, 0, 0);
            break;
        case GP_LOC_KEY(2, 30, 0, 0):
            Task_SpawnFromTable(D_80182D0C, 0, 1, 0);
            break;
        case GP_LOC_KEY(3, 30, 0, 0):
            Task_SpawnFromTable(D_80182E74, 0, 1, 0);
            break;
        case GP_LOC_KEY(4, 18, 0, 0):
            Task_SpawnFromTable(D_80181B88, 0, 0, 0);
            break;
        case GP_LOC_KEY(5, 31, 0, 0):
            Task_SpawnFromTable(D_8017D9E8, 0, 0, 0);
            break;
        case GP_LOC_KEY(5, 30, 0, 0):
            Task_SpawnFromTable(D_8018186C, 0, 0, 0);
            Task_SpawnFromTable(D_8018186C, 1, 0, 0);
            break;
        case GP_LOC_KEY(5, 29, 0, 0):
            Task_SpawnFromTable(D_80181638, 0, 0, 0);
            break;
        case GP_LOC_KEY(4, 22, 0, 0):
            Task_SpawnFromTable(D_801637C8, 0, 0, 0);
            break;
        case GP_LOC_KEY(4, 48, 0, 0):
            Task_SpawnFromTable(D_80182FAC, 0, 0, 0);
            break;
        case GP_LOC_KEY(1, 20, 0, 0):
            func_8017FBD8();
            break;
    }
}

void Gp_BlendRgb555Clut(u16* arg0, u16* arg1, s32 arg2, u16* arg3)
{
    s32 i;

    for (i = 0; i < 0x10; i++) {
        Gp_BlendRgb555(arg0, arg1, arg2, arg3);
        arg0++;
        arg1++;
        arg3++;
    }
}

static void Gp_BlendRgb555ClutMasked(u16* arg0, u16* arg1, s32 arg2, u16* arg3, s32 arg4)
{
    s32 i;

    for (i = 0; i < 0x10; i++) {
        if ((1 << i) & arg4) {
            Gp_BlendRgb555(arg0, arg1, arg2, arg3);
        }
        arg0++;
        arg1++;
        arg3++;
    }
}

static void func_800B28E0(Task* task)
{
    task->killCountdown = 0x20;
    task->state++;
    func_800B1EFC(task);
}

void func_800B2910(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80093A38;
    sp.funcs[arg0->state](arg0);
}

Task* func_800B2968(void)
{
    return Task_SpawnFromTable(D_80119218, 0, 0, 0);
}

static void Gp_BlendAnimRot(GpAnimBlendSrc* arg0, GpCoord* arg1, GpAnimSlot* arg2,
                            GpAnimScratch80* s)
{
    if (arg2->bufPose != 0) {
        RotMatrix_gte(&s->vec0, &s->mtx0);
        if (arg0->field_10 == 1) {
            RotMatrix_gte(&s->vec1, &s->mtx1);
            TransposeMatrix(&s->mtx0, &s->mtx2);
            gte_MulMatrix0(&s->mtx1, &s->mtx2, &s->mtx2);
            Gfx_MatrixToEuler(&s->mtx2, &arg2->bufRotDelta);
        }
        gte_lddp(s->invBlend);
        gte_ldsv(&arg2->bufRotDelta);
        gte_gpf12();
        gte_stsv(&s->vec1);
        RotMatrix_gte(&s->vec1, &s->mtx2);
        if (arg0->field_C == NULL) {
            gte_MulMatrix0(&s->mtx2, &s->mtx0, &arg1->coord);
            if (arg0->field_8 != NULL) {
                Gfx_MatrixToEuler(&arg1->coord, &s->vec1);
            }
            arg1->flg = 0;
        } else {
            gte_MulMatrix0(&s->mtx2, &s->mtx0, &s->mtx2);
            Gfx_MatrixToEuler(&s->mtx2, &s->vec1);
            arg0->field_C->rot = s->vec1;
        }
    } else {
        gte_lddp(s->blend);
        gte_ldsv(&s->vec0);
        gte_gpf12();
        gte_lddp(s->invBlend);
        gte_ldsv(&s->vec1);
        gte_gpl12();
        gte_stsv(&s->vec1);
        if (arg0->field_C == NULL) {
            RotMatrix_gte(&s->vec1, &arg1->coord);
            arg1->flg = 0;
        } else {
            arg0->field_C->rot = s->vec1;
        }
    }
}

static void Gp_AnimBlendPose(GpAnimBlendSrc* arg0, GpCoord* arg1, GpAnimSlot* arg2)
{
    GpAnimScratch80* s;
    GpPackedPose*    pose;
    s32              blend;

    if (arg2->timeSpan != 0) {
        s = SCRATCH_PUSH(GpAnimScratch80);
        if (arg0->field_0 != arg0->field_4) {
            blend       = arg2->timeLeft << 12;
            s->blend    = blend;
            blend       = blend / arg2->timeSpan;
            s->blend    = blend;
            s->invBlend = 0x1000 - blend;
        } else {
            s->blend    = 0;
            s->invBlend = 0x1000;
        }
        gte_lddp(s->blend);
        gte_ldsv(arg0->field_0);
        gte_gpf12();
        gte_lddp(s->invBlend);
        gte_ldsv(arg0->field_4);
        gte_gpl12();
        gte_stsv(&s->trans);
        if (arg0->field_C == NULL) {
            arg1->coord.t[0] = s->trans.vx;
            arg1->coord.t[1] = s->trans.vy;
            arg1->coord.t[2] = s->trans.vz;
            arg1->flg        = 0;
        } else {
            arg0->field_C->trans.vx = s->trans.vx;
            arg0->field_C->trans.vy = s->trans.vy;
            arg0->field_C->trans.vz = s->trans.vz;
        }
        pose       = (GpPackedPose*)arg0->field_0;
        s->vec0.vx = pose->rx;
        s->vec0.vy = pose->ry;
        s->vec0.vz = pose->rz;
        pose       = (GpPackedPose*)arg0->field_4;
        s->vec1.vx = pose->rx;
        s->vec1.vy = pose->ry;
        s->vec1.vz = pose->rz;
        Gp_BlendAnimRot(arg0, arg1, arg2, s);
        pose = (GpPackedPose*)arg0->field_8;
        if (pose != NULL) {
            pose->vx = s->trans.vx;
            pose->vy = s->trans.vy;
            pose->vz = s->trans.vz;
            pose->rx = s->vec1.vx;
            pose->ry = s->vec1.vy;
            pose->rz = s->vec1.vz;
        }
        SCRATCH_POP(GpAnimScratch80);
    }
}

static void Gp_AnimBlendPacked(GpAnimBlendSrc* arg0, GpCoord* arg1, GpAnimSlot* arg2)
{
    GpAnimScratch80* s;
    GpPackedSvec*    p;
    GpPackedSvec*    dest;
    s32              blend;

    if (arg2->timeSpan != 0) {
        s = SCRATCH_PUSH(GpAnimScratch80);
        if (arg0->field_0 != arg0->field_4) {
            blend       = arg2->timeLeft << 12;
            s->blend    = blend;
            blend       = blend / arg2->timeSpan;
            s->blend    = blend;
            s->invBlend = 0x1000 - blend;
        } else {
            s->blend    = 0;
            s->invBlend = 0x1000;
        }
        p          = arg0->field_0;
        s->vec0.vx = p->rx << 3;
        s->vec0.vy = p->ry << 3;
        s->vec0.vz = p->rz << 3;
        p          = arg0->field_4;
        s->vec1.vx = p->rx << 3;
        s->vec1.vy = p->ry << 3;
        s->vec1.vz = p->rz << 3;
        Gp_BlendAnimRot(arg0, arg1, arg2, s);
        dest = arg0->field_8;
        if (dest != NULL) {
            dest->rx = s->vec1.vx >> 3;
            dest->ry = s->vec1.vy >> 3;
            dest->rz = s->vec1.vz >> 3;
        }
        SCRATCH_POP(GpAnimScratch80);
    }
}

static void Gp_AnimAdvanceSlot(GpAnimCtx* arg0, s32 arg1)
{
    GpAnimSlot* slot;
    GpAnimSet** sets;
    GpAnimRec*  recs;
    GpAnimRec*  rec;
    u16         idx;
    s32         setIdx;
    u16         val;

    slot        = &arg0->slots[arg1];
    slot->flags = 0;
    if (*(s32*)&slot->curSet != *(s32*)&slot->nextSet) {
        sets = slot->sets;
        do {
            *(s32*)&slot->curSet = *(s32*)&slot->nextSet;
            idx                  = slot->nextRec + 1;
            setIdx               = slot->nextSet;
            recs                 = sets[setIdx]->recs;
            while ((s8)recs[idx].flags < 0) {
                rec = (GpAnimRec*)((idx << 2) + (s32)recs);
                if (rec->flags < 0xC0) {
                    idx = rec->pose;
                    if (idx == slot->nextRec) {
                        slot->flags |= 1;
                    }
                    slot->flags |= 2;
                } else {
                    idx          = slot->nextRec;
                    slot->flags |= 1;
                    break;
                }
            }
            slot->nextRec = idx;
            slot->nextSet = setIdx;
            if (slot->flags & 3) {
                break;
            }
        } while (*(s32*)&slot->curSet != *(s32*)&slot->nextSet);
    }

    val            = slot->sets[slot->nextSet]->recs[slot->nextRec].duration << 4;
    slot->timeSpan = val;
    slot->timeLeft = val;
    func_800B3448(arg0, arg1, 0, 0);
}

void func_800B3448(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GpAnimScratch18* s;
    GpAnimSlot*      slot;
    GpCoord*         coord;
    GpAnimSet*       set;
    GpAnimRec*       recs;
    GpAnimRec*       rec;
    GpPackedSvec*    poses;
    u16              idx;
    u16              idx2;
    s32              setIdx;
    s32              setIdx2;
    u16              lim;
    u16              val;
    s16              rem;
    s32              op;
    u16              base;

    slot  = &arg0->slots[arg1];
    coord = &arg0->coords[slot->mtxIndex];
    SCRATCH_PUSH(GpAnimScratch18);
    s           = SCRATCH_HEAD(GpAnimScratch18);
    slot->flags = 0;
    if (slot->atEnd == 1) {
        if (*(s32*)&slot->nextSet == *(s32*)&slot->curSet) {
            slot->flags = 0x100;
        } else {
            slot->atEnd = 0;
        }
    } else {
        if (gGameSession->deathVariant != 0) {
            base           = slot->timeLeft - 1;
            slot->timeLeft = base - (((s8)slot->rate - 1) >> 1);
        } else {
            slot->timeLeft -= (s8)slot->rate;
        }
    }

    rem = slot->timeLeft;
    if (rem <= 0) {
        slot->field_A = 0;
        while (slot->timeLeft <= 0) {
            *(s32*)&slot->curSet = *(s32*)&slot->nextSet;
            idx                  = slot->nextRec + 1;
            setIdx               = slot->nextSet;
            recs                 = slot->sets[setIdx]->recs;
            while ((s8)recs[idx].flags < 0) {
                rec = (GpAnimRec*)((idx << 2) + (s32)recs);
                if (rec->flags < 0xC0) {
                    idx = rec->pose;
                    if (idx == slot->nextRec) {
                        slot->flags |= 1;
                    }
                    slot->flags |= 2;
                } else {
                    idx          = slot->nextRec;
                    slot->flags |= 1;
                    break;
                }
            }
            slot->nextSet   = setIdx;
            slot->nextRec   = idx;
            recs            = slot->sets[slot->nextSet]->recs;
            val             = recs[slot->nextRec].duration << 4;
            slot->timeSpan  = val;
            slot->timeLeft += val;
        }
        if (slot->flags & 1) {
            slot->atEnd  = 1;
            slot->flags |= 0x100;
        } else {
            slot->atEnd = 0;
        }
    } else if (slot->timeSpan < rem) {
        slot->field_A = 0;
        while (slot->timeLeft > slot->timeSpan) {
            slot->timeLeft       -= slot->timeSpan;
            *(s32*)&slot->nextSet = *(s32*)&slot->curSet;
            setIdx2               = slot->curSet;
            idx2                  = slot->curRec - 1;
            lim                   = slot->sets[setIdx2]->trackStart[slot->trackIndex];
            if (idx2 < lim) {
                idx2         = lim;
                slot->flags |= 1;
            }
            slot->curRec   = idx2;
            slot->curSet   = setIdx2;
            recs           = slot->sets[slot->nextSet]->recs;
            val            = recs[slot->nextRec].duration << 4;
            slot->timeSpan = val;
        }
        if (slot->flags & 1) {
            slot->atEnd  = 1;
            slot->flags |= 0x100;
        } else {
            slot->atEnd = 0;
        }
    }

    op              = slot->poseKind;
    s->src.field_10 = slot->bufPose;
    slot->bufPose   = 0;
    if (slot->curSet == 0x7FFF) {
        s->src.field_0 = (GpPackedSvec*)((s32)arg0->poses + (arg1 << 4));
        slot->bufPose  = 1;
    } else {
        recs           = slot->sets[slot->curSet]->recs;
        poses          = slot->sets[slot->curSet]->poseBanks[op];
        s->src.field_0 = &poses[recs[slot->curRec].pose];
    }
    if (slot->nextSet == 0x7FFF) {
        s->src.field_4 = (GpPackedSvec*)((s32)arg0->poses + (arg1 << 4));
        slot->bufPose  = 1;
    } else {
        set            = slot->sets[slot->nextSet];
        recs           = set->recs;
        poses          = set->poseBanks[op];
        s->src.field_4 = &poses[recs[slot->nextRec].pose];
    }
    if ((s->src.field_10 == 0) && (slot->bufPose == 1)) {
        s->src.field_10 = slot->bufPose;
    } else {
        s->src.field_10 = 0;
    }
    s->src.field_8 = (GpPackedSvec*)arg3;
    s->src.field_C = (GpAnimPose*)arg2;
    switch (op) {
        case 1:
            Gp_AnimBlendPose(&s->src, coord, slot);
            break;
        case 2:
            printf(D_80093A44);
            break;
        case 4:
            Gp_AnimBlendPacked(&s->src, coord, slot);
            break;
    }
    SCRATCH_POP(GpAnimScratch18);
}

static inline void _gpAnimSeekSlot(GpAnimCtx* arg0, s32 arg1, u16 arg2, s32 arg3, s32 arg4)
{
    GpAnimSlot* slot;
    GpAnimSet*  set;
    GpAnimRec*  recs;
    GpAnimRec*  rec;
    u16         idx;
    u16         val;
    s32         off;

    off  = arg1 << 4;
    slot = &arg0->slots[arg1];
    func_800B3448(arg0, arg1, 0, (s32)arg0->poses + off);
    slot->curSet = 0x7FFF;
    set          = slot->sets[arg2];
    recs         = set->recs;
    idx          = set->trackStart[slot->trackIndex] + arg3;
    while ((s8)recs[idx].flags < 0) {
        rec = (GpAnimRec*)((idx << 2) + (s32)recs);
        if (rec->flags < 0xC0) {
            idx = rec->pose;
            if (idx == slot->nextRec) {
                slot->flags |= 1;
            }
            slot->flags |= 2;
        } else {
            idx          = slot->nextRec;
            slot->flags |= 1;
            break;
        }
    }
    slot->nextRec  = idx;
    slot->nextSet  = arg2;
    val            = arg4 << 4;
    slot->timeSpan = val;
    slot->timeLeft = val;
    slot->bufPose  = 0;
}

static void Gp_AnimSeekSlotEx(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GpAnimSlot* slot;
    GpAnimRec*  recs;
    u16         val;

    slot = &arg0->slots[arg1];
    recs = slot->sets[arg2]->recs;
    _gpAnimSeekSlot(arg0, arg1, arg2, arg3, 1);
    val            = recs[slot->nextRec].duration << 4;
    slot->timeSpan = val;
    slot->timeLeft = val;
}

void func_800B3AA4(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5)
{
    GpAnimSlot* slot;
    GpAnimSet** sets;
    GpAnimSet*  set;
    GpAnimRec*  recs;
    GpAnimRec*  rec;
    u16         recIdx;
    u16         val;
    u8          op;
    s32         setIdx;

    if (Mc_SaveData[0].demoScene == 1) {
        u8  idx;
        s32 off;

        idx            = arg1->trackIndex;
        setIdx         = arg3;
        arg0->slots    = arg1 - idx;
        arg1->mtxIndex = arg2;
        idx            = arg1->trackIndex;
        off            = idx << 4;
        slot           = &arg0->slots[idx];
        func_800B3448(arg0, idx, 0, (s32)arg0->poses + off);
        slot->curSet = 0x7FFF;
        set          = slot->sets[(u16)setIdx];
        recs         = set->recs;
        recIdx       = set->trackStart[slot->trackIndex] + arg4;
        while ((s8)recs[recIdx].flags < 0) {
            rec = (GpAnimRec*)((recIdx << 2) + (s32)recs);
            if (rec->flags < 0xC0) {
                recIdx = rec->pose;
                if (recIdx == slot->nextRec) {
                    slot->flags |= 1;
                }
                slot->flags |= 2;
            } else {
                recIdx       = slot->nextRec;
                slot->flags |= 1;
                break;
            }
        }
        slot->nextRec  = recIdx;
        slot->nextSet  = setIdx;
        val            = arg5 << 4;
        slot->timeSpan = val;
        slot->timeLeft = val;
        slot->bufPose  = 0;
    } else {
        if (arg3 == 0) {
            arg3 = 1;
        } else if (arg3 < 0) {
            arg3 = -arg3;
        }

        arg1->rate       = 0x10;
        arg1->timeLeft   = 0;
        arg1->curSet     = arg3;
        arg1->curRec     = 0;
        arg1->mtxIndex   = arg2;
        arg1->trackIndex = arg2;
        arg1->nextSet    = arg3;
        sets             = arg0->sets;
        arg1->sets       = sets;
        arg1->nextRec    = sets[arg3]->trackStart[arg1->trackIndex];
        arg1->curRec     = arg1->sets[arg3]->trackStart[arg1->trackIndex];
        op               = arg1->sets[arg1->nextSet]->recs[arg1->nextRec].flags;
        arg1->field_12   = 0;
        arg1->flags      = 0;
        arg1->atEnd      = 0;
        arg1->poseKind   = op & 0xF;
    }
}

void Gp_AnimInitCtx(GpAnimCtx* arg0, void* arg1, TmdObject* arg2, void* arg3)
{
    arg0->sets      = arg1;
    arg0->coords    = (GpCoord*)(arg2 + 1);
    arg0->poses     = arg3;
    arg0->partCount = arg2->partCount;
}

void Gp_AnimInitSlot(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3)
{
    GpAnimSet** sets;
    u8          op;

    if (arg3 == 0) {
        arg3 = 1;
    } else if (arg3 < 0) {
        arg3 = -arg3;
    }

    arg1->rate       = 0x10;
    arg1->timeLeft   = 0;
    arg1->curSet     = arg3;
    arg1->curRec     = 0;
    arg1->mtxIndex   = arg2;
    arg1->trackIndex = arg2;
    arg1->nextSet    = arg3;
    sets             = arg0->sets;
    arg1->sets       = sets;
    arg1->nextRec    = sets[arg3]->trackStart[arg1->trackIndex];
    arg1->curRec     = arg1->sets[arg3]->trackStart[arg1->trackIndex];
    op               = arg1->sets[arg1->nextSet]->recs[arg1->nextRec].flags;
    arg1->field_12   = 0;
    arg1->flags      = 0;
    arg1->atEnd      = 0;
    arg1->poseKind   = op & 0xF;
}

void Gp_AnimTickSlot(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u8 idx;

    idx         = arg1->trackIndex;
    arg0->slots = arg1 - idx;
    func_800B3448(arg0, idx, 0, 0);
}

void Gp_AnimTickSlot2(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u8 idx;

    idx         = arg1->trackIndex;
    arg0->slots = arg1 - idx;
    func_800B3448(arg0, idx, 0, 0);
}

static void Gp_AnimTickSlot3(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u8 idx;

    idx         = arg1->trackIndex;
    arg0->slots = arg1 - idx;
    func_800B3448(arg0, idx, 0, 0);
}

static void func_800B3E74(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3)
{
    GpAnimRec* recs;
    u16        val;

    recs = arg1->sets[arg3]->recs;
    func_800B3AA4(arg0, arg1, arg2, arg3, 0, 8);
    val            = recs[arg1->nextRec].duration << 4;
    arg1->timeSpan = val;
    arg1->timeLeft = val;
}

static void func_800B3EE8(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3, s32 arg4)
{
    GpAnimRec* recs;
    u16        val;

    recs = arg1->sets[arg3]->recs;
    func_800B3AA4(arg0, arg1, arg2, arg3, arg4, 8);
    val            = recs[arg1->nextRec].duration << 4;
    arg1->timeSpan = val;
    arg1->timeLeft = val;
}

void Gp_AnimInitCtxSlots(GpAnimCtx* arg0, void* arg1, TmdObject* arg2, void* arg3, GpAnimSlot* arg4)
{
    arg0->sets      = arg1;
    arg0->coords    = (GpCoord*)(arg2 + 1);
    arg0->poses     = arg3;
    arg0->partCount = arg2->partCount;
    arg0->slots     = arg4;
}

void func_800B3F84(GpAnimCtx* arg0, void* arg1, TmdObject* arg2, void* arg3, GpAnimSlot* arg4)
{
    Gp_AnimInitCtxSlots(arg0, arg1, arg2, arg3, arg4);
}

void Gp_AnimResetSlot(GpAnimCtx* arg0, s32 arg1, s32 arg2)
{
    GpAnimSlot* slot;
    GpAnimSet** sets;
    u8          op;

    slot             = &arg0->slots[arg1];
    slot->rate       = 0x10;
    slot->timeLeft   = 0;
    slot->curSet     = arg2;
    slot->curRec     = 0;
    slot->mtxIndex   = arg1;
    slot->trackIndex = arg1;
    slot->nextSet    = arg2;
    sets             = arg0->sets;
    slot->sets       = sets;
    slot->nextRec    = sets[arg2]->trackStart[slot->trackIndex];
    op               = slot->sets[slot->nextSet]->recs[slot->nextRec].flags;
    slot->flags      = 0;
    slot->atEnd      = 0;
    slot->field_12   = 0;
    slot->poseKind   = op & 0xF;
}

void Gp_AnimResetSlotEx(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    GpAnimSlot* slot;
    GpAnimSet** sets;
    u8          op;

    slot             = &arg0->slots[arg1];
    slot->rate       = 0x10;
    slot->timeLeft   = 0;
    slot->curSet     = arg2;
    slot->curRec     = 0;
    slot->mtxIndex   = arg4;
    slot->trackIndex = arg3;
    slot->nextSet    = arg2;
    sets             = arg0->sets;
    slot->sets       = sets;
    slot->nextRec    = sets[arg2]->trackStart[slot->trackIndex];
    op               = slot->sets[slot->nextSet]->recs[slot->nextRec].flags;
    slot->flags      = 0;
    slot->atEnd      = 0;
    slot->field_12   = 0;
    slot->poseKind   = op & 0xF;
}

static void Gp_AnimSeekSlot(GpAnimCtx* arg0, s32 arg1, s32 arg2)
{
    Gp_AnimSeekSlotEx(arg0, arg1, arg2, 0);
}

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    _gpAnimSeekSlot(arg0, arg1, arg2, arg3, arg4);
}

void Gp_AnimWritePoseBlend(GpAnimCtx* arg0, s32 arg1, GpAnimPose* arg2, GpAnimPose* arg3, s32 arg4,
                           s32 arg5)
{
    void**      scratch;
    GpAnimPose* head;
    GpAnimSlot* slot;
    GpCoord*    dest;
    SVECTOR*    trans;
    SVECTOR*    rot;
    s32         idx;

    scratch                        = SCRATCH_HEAD_ADDR;
    slot                           = &arg0->slots[arg1];
    head                           = SCRATCH_HEAD_AT(scratch, GpAnimPose);
    idx                            = slot->mtxIndex;
    SCRATCH_HEAD_AT(scratch, void) = head - 1;
    dest                           = &arg0->coords[idx];
    trans                          = &head[-1].trans;
    if (slot->poseKind == 1) {
        gte_lddp(arg4);
        gte_ldsv(&arg2->trans);
        gte_gpf12();
        gte_lddp(arg5);
        gte_ldsv(&arg3->trans);
        gte_gpl12();
        gte_stsv(trans);
        dest->coord.t[0] = trans->vx;
        dest->coord.t[1] = trans->vy;
        dest->coord.t[2] = trans->vz;
    }
    gte_lddp(arg4);
    gte_ldsv(&arg2->rot);
    gte_gpf12();
    gte_lddp(arg5);
    gte_ldsv(&arg3->rot);
    gte_gpl12();
    rot = &head[-1].rot;
    gte_stsv(rot);
    RotMatrix_gte(rot, &dest->coord);
    dest->flg = 0;
    SCRATCH_POP(GpAnimPose);
}

void Gp_AnimWritePoseCopy(GpAnimCtx* arg0, s32 arg1, GpAnimPose* arg2, GpAnimPose* arg3, s32 arg4,
                          s32 arg5)
{
    void**      scratch;
    GpAnimPose* head;
    GpAnimSlot* slot;
    GpCoord*    dest;
    SVECTOR*    rot;
    s32         idx;

    scratch                        = SCRATCH_HEAD_ADDR;
    slot                           = &arg0->slots[arg1];
    head                           = SCRATCH_HEAD_AT(scratch, GpAnimPose);
    idx                            = slot->mtxIndex;
    SCRATCH_HEAD_AT(scratch, void) = head - 1;
    dest                           = &arg0->coords[idx];
    if (slot->poseKind == 1) {
        dest->coord.t[0] = arg2->trans.vx;
        dest->coord.t[1] = arg2->trans.vy;
        dest->coord.t[2] = arg2->trans.vz;
    }
    gte_lddp(arg4);
    gte_ldsv(&arg2->rot);
    gte_gpf12();
    gte_lddp(arg5);
    gte_ldsv(&arg3->rot);
    gte_gpl12();
    rot = &head[-1].rot;
    gte_stsv(rot);
    RotMatrix_gte(rot, &dest->coord);
    dest->flg = 0;
    SCRATCH_POP(GpAnimPose);
}

void Gp_AnimTickIndex(GpAnimCtx* arg0, s32 arg1)
{
    func_800B3448(arg0, arg1, 0, 0);
}

void func_800B4538(GpAnimCtx* arg0, s32 arg1, s32 arg2, u16 arg3, s32 arg4, s32 arg5, s32 arg6)
{
    GpAnimSlot* slot;
    GpAnimSet*  set;
    GpAnimRec*  recs;
    GpAnimRec*  rec;
    u16         idx;
    u16         val;
    s32         off;

    off  = arg1 << 4;
    slot = &arg0->slots[arg1];
    func_800B3448(arg0, arg1, arg2, (s32)arg0->poses + off);
    slot->curSet = 0x7FFF;
    set          = slot->sets[arg3];
    recs         = set->recs;
    idx          = set->trackStart[slot->trackIndex] + arg4;
    while ((s8)recs[idx].flags < 0) {
        rec = (GpAnimRec*)((idx << 2) + (s32)recs);
        if (rec->flags < 0xC0) {
            idx = rec->pose;
            if (idx == slot->nextRec) {
                slot->flags |= 1;
            }
            slot->flags |= 2;
        } else {
            idx          = slot->nextRec;
            slot->flags |= 1;
            break;
        }
    }
    slot->nextRec  = idx;
    slot->nextSet  = arg3;
    val            = arg6 << 4;
    slot->timeSpan = val;
    slot->timeLeft = val;
    slot->bufPose  = 0;
}

GpAnimRec* Gp_AnimGetRec(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u16        idx;
    GpAnimRec* ret;

    idx = arg1->curSet;
    switch (idx) {
        case 0x7FFF:
            return NULL;
        default:
            ret  = arg1->sets[idx]->recs;
            ret += arg1->curRec;
            return ret;
    }
}

static void func_800B46A4(GpAnimCtx* arg0, GpAnimSlot* arg1, u16 arg2, u16 arg3)
{
    GpAnimRec* recs;
    GpAnimRec* rec;

    recs = arg1->sets[arg2]->recs;
    while ((s8)recs[arg3].flags < 0) {
        rec = (GpAnimRec*)((arg3 << 2) + (s32)recs);
        if (rec->flags < 0xC0) {
            arg3 = rec->pose;
            if (arg3 == arg1->nextRec) {
                arg1->flags |= 1;
            }
            arg1->flags |= 2;
        } else {
            arg3         = arg1->nextRec;
            arg1->flags |= 1;
            break;
        }
    }
    arg1->nextRec = arg3;
    arg1->nextSet = arg2;
}

static void func_800B4754(GpAnimCtx* arg0, GpAnimSlot* arg1, u16 arg2, u16 arg3)
{
    u16 limit;

    limit = arg1->sets[arg2]->trackStart[arg1->trackIndex];
    if (arg3 < limit) {
        arg3         = limit;
        arg1->flags |= 1;
    }
    arg1->curRec = arg3;
    arg1->curSet = arg2;
}

void Gp_AnimPlaySlot(GpAnimCtx* arg0, s32 arg1, s32 arg2, u16 arg3, s32 arg4, s32 arg5, s32 arg6,
                     void* arg7)
{
    GpAnimSlot* slot;
    GpAnimSet*  set;
    GpAnimRec*  recs;
    GpAnimRec*  rec;
    u16         idx;
    u16         val;
    s32         off;

    off  = arg1 << 4;
    slot = &arg0->slots[arg1];
    func_800B3448(arg0, arg1, arg2, (s32)arg0->poses + off);
    slot->curSet = 0x7FFF;
    if (arg7 != NULL) {
        arg0->sets = arg7;
        slot->sets = arg7;
    }
    set  = slot->sets[arg3];
    recs = set->recs;
    /* arg4 is an offset into the track's records; rebase it to a record index. */
    arg4 = (u16)(set->trackStart[slot->trackIndex] + arg4);
    idx  = arg4;
    while ((s8)recs[idx].flags < 0) {
        rec = (GpAnimRec*)((idx << 2) + (s32)recs);
        if (rec->flags < 0xC0) {
            idx = rec->pose;
            if (idx == slot->nextRec) {
                slot->flags |= 1;
            }
            slot->flags |= 2;
        } else {
            idx          = slot->nextRec;
            slot->flags |= 1;
            break;
        }
    }
    slot->nextRec  = idx;
    slot->nextSet  = arg3;
    val            = arg6 << 4;
    slot->timeSpan = val;
    slot->timeLeft = val;
    slot->bufPose  = 0;
}

void Gp_SaveEnemyPose(GpEnemy* arg0)
{
    McPosRec*  rec;
    GpAreaKey* loc;
    TmdObject* extra;
    GpCoord*   coord;
    SVECTOR*   euler;
    u16        id;
    s32        i;

    rec   = Mc_SaveData[0].enemyPoses;
    loc   = (GpAreaKey*)&Mc_SaveData[0].at4.loc.view;
    extra = arg0->task->extra.tmd;
    coord = extra->coords;
    if (arg0->spawnState == 0) {
        arg0->spawnState = 1;
    }
    id = arg0->placeKey;
    for (i = 0; i < 0x20; i++, rec++) {
        if (rec->placeKey == id) {
            return;
        }
    }

    euler = SCRATCH_PUSH(SVECTOR);
    rec   = Mc_SaveData[0].enemyPoses;
    for (i = 0; i < 0x20; i++, rec++) {
        if (rec->spawnState == 0) {
            break;
        }
    }
    if (i == 0x20) {
        u32 key;

        rec = Mc_SaveData[0].enemyPoses;
        key = (loc->stage << 8) | loc->area;
        for (i = 0; i < 0x1F; i++, rec++) {
            if ((rec->placeKey & 0xFFF) != key) {
                break;
            }
        }
        for (; i < 0x1F; i++, rec++) {
            rec[0] = rec[1];
        }
    }
    rec->spawnState = arg0->spawnState;
    rec->placeKey   = arg0->placeKey;
    rec->x          = coord->coord.t[0];
    rec->y          = coord->coord.t[1];
    rec->z          = coord->coord.t[2];
    Gfx_MatrixToEuler(&coord->coord, euler);
    euler->vx  = euler->vx >> 8;
    rec->pitch = euler->vx;
    euler->vy  = euler->vy >> 8;
    rec->yaw   = euler->vy;
    euler->vz  = euler->vz >> 8;
    rec->roll  = euler->vz;
    SCRATCH_POP(SVECTOR);
}

void Gp_SpawnArea(GpAreaKey* arg0)
{
    GpAreaRec*     recs;
    GpAreaVariant* nested;
    GpAreaObj*     obj;
    GpAreaPlace*   place;
    GpAreaTmdRec*  entry;
    GpEnemy*       enemy;
    Task*          task;
    TmdObject*     extra;
    GpCoord*       coord;
    u16            id;
    s8             placeNo;
    s32            i;

    recs = Gp_AreaTables[arg0->stage];
    Gp_ResetLinkState();
    if (recs == NULL) {
        return;
    }
    nested = recs[arg0->area].field_0;
    obj    = recs[arg0->area].field_4;
    if (nested == NULL) {
        return;
    }
    func_800B5A48(arg0, obj);
    place   = nested[arg0->place].field_0;
    placeNo = 0;
    if (place == NULL) {
        return;
    }
    if (place->entryId == 0xFF) {
        return;
    }
    do {
        entry = nested[arg0->place].field_4;
        id    = entry->field_0;
        if (id != 0xFF) {
            do {
                if (id == place->entryId) {
                    if (obj->field_1 & 2) {
                        McPosRec* rec;
                        s32       found;
                        s32       j;

                        rec   = Mc_SaveData[0].enemyPoses;
                        found = 0;
                        for (j = 0; j < 0x20; j++, rec++) {
                            if (rec->placeKey == ((placeNo << 12) | (arg0->stage << 8) | arg0->area)) {
                                found = 1;
                                break;
                            }
                        }
                        if (found == 0) {
                            break;
                        }
                    }
                    enemy = Gp_SpawnEnemyFromTable(entry->field_8, entry->field_5,
                                                   (place->variant << 16) | place->mode, NULL);
                    if (enemy != NULL) {
                        u16 key;

                        key             = (placeNo << 12) | (arg0->stage << 8) | arg0->area;
                        enemy->workType = 0x900;
                        enemy->place    = place;
                        enemy->placeKey = key;
                        task            = enemy->task;
                        if (task->spawnType != 0) {
                            extra = task->extra.tmd;
                            coord = extra->coords;
                            if (task->spawnType == 1) {
                                extra->tpage = place->tpage;
                                extra->clut  = place->clut;
                                if (extra->buffer != NULL) {
                                    tmdProcessStream(extra);
                                    tmdProcessStream(extra);
                                }
                            }
                            if (!(obj->field_1 & 2)) {
                                coord->coord.t[0]   = place->x;
                                coord->coord.t[1]   = place->y;
                                coord->coord.t[2]   = place->z;
                                coord->param.rot.vy = place->yaw;
                                Gfx_RotMatrixY(&coord->coord, place->yaw, 1);
                            } else {
                                McPosRec* rec;

                                rec = Mc_SaveData[0].enemyPoses;
                                i   = 0;
                                do {
                                    if (rec->placeKey == enemy->placeKey) {
                                        coord->coord.t[0]   = rec->x;
                                        coord->coord.t[1]   = rec->y;
                                        coord->coord.t[2]   = rec->z;
                                        coord->param.rot.vx = rec->pitch << 8;
                                        coord->param.rot.vy = rec->yaw << 8;
                                        coord->param.rot.vz = rec->roll << 8;
                                        RotMatrix_gte(&coord->param.rot,
                                                      &coord->coord);
                                        enemy->spawnState = rec->spawnState;
                                        break;
                                    }
                                    i++;
                                    rec++;
                                } while (i < 0x20);
                                if (i == 0x20) {
                                    Gp_DestroyEnemy(enemy, enemy->task);
                                }
                            }
                        }
                    }
                    break;
                }
                entry++;
                id = entry->field_0;
            } while (id != 0xFF);
        }
        placeNo++;
        place++;
    } while (place->entryId != 0xFF);
}

void Gp_DrawFloorQuad(GpCoord* arg0, u32 arg1, SVECTOR* arg2)
{
    GpFloorQuadScratch* block;
    POLY_FT4*           prim;

    block = SCRATCH_PUSH(GpFloorQuadScratch);
    if (arg2 == NULL) {
        block->vec[0].vx = -(arg1 >> 1);
        block->vec[0].vy = 0;
        block->vec[0].vz = -(arg1 >> 1);
    } else {
        block->vec[0].vx = arg2->vx - (arg1 >> 1);
        block->vec[0].vy = arg2->vy;
        block->vec[0].vz = arg2->vz - (arg1 >> 1);
    }
    block->vec[3].vy = block->vec[2].vy = block->vec[1].vy = block->vec[0].vy;
    block->vec[1].vx = block->vec[3].vx = block->vec[0].vx + arg1;
    block->vec[2].vx                    = block->vec[0].vx;
    block->vec[2].vz = block->vec[3].vz = block->vec[0].vz + arg1;
    block->vec[1].vz                    = block->vec[0].vz;
    Gp_UpdateCoord(arg0);
    gte_SetRotMatrix(&arg0->workm);
    gte_SetTransMatrix(&arg0->workm);
    block->maxotz = 0;

    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->otz > block->maxotz) {
        block->maxotz = block->otz;
    }

    gte_ldv0(&block->vec[1]);
    gte_rtps();
    gte_stsxy(&block->sxy1);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->otz > block->maxotz) {
        block->maxotz = block->otz;
    }

    gte_ldv0(&block->vec[2]);
    gte_rtps();
    gte_stsxy(&block->sxy2);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->otz > block->maxotz) {
        block->maxotz = block->otz;
    }

    gte_ldv0(&block->vec[3]);
    gte_rtps();
    gte_stsxy(&block->sxy3);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->otz > block->maxotz) {
        block->maxotz = block->otz;
    }

    if (block->flag >= 0) {
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        PRIM_XY_WORD(prim, 0) = *(u32*)&block->sxy0;
        PRIM_XY_WORD(prim, 1) = *(u32*)&block->sxy1;
        PRIM_XY_WORD(prim, 2) = *(u32*)&block->sxy2;
        PRIM_XY_WORD(prim, 3) = *(u32*)&block->sxy3;
        setUV4(prim, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
        prim->tpage = 0x48;
        prim->g0    = 0xC0;
        prim->b0    = 0xC0;
        prim->r0    = 0xC0;
        prim->clut  = 0x4283;
        addPrim(&gGpuCurrentOt[block->maxotz >> 4], prim);
    }
    SCRATCH_POP(GpFloorQuadScratch);
}

static void func_800B51F4(Task* task)
{
    s32       count;
    s32       i;
    s32       x;
    s32       cx;
    s32       y;
    s32       mode;
    u8        flag;
    u16       flag2;
    s32       color;
    s32       right;
    TILE*     tile;
    DR_TPAGE* dr;
    DR_TPAGE* fadeDr;
    DR_STP*   stp;
    POLY_FT4* p0;
    POLY_FT4* p1;

    mode  = gDisplayState.drawBuffer;
    count = 1;
    x     = 0;
    y     = 0;
    cx    = 0;
    if (task->spawnArg1 == 0x10) {
        count = 2;
    }
    if (Mc_SaveData[0].demoScene == 1) {
        return;
    }

    if (task->spawnArg1 & 1) {
        task->killCountdown++;
        if (task->killCountdown >= 0x3D) {
            color          = task->killCountdown - 0x3C;
            color         *= 8;
            tile           = (TILE*)gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setlen(tile, 3);
            setcode(tile, 0x62);
            if (color >= 0x100) {
                color = 0xFF;
            }
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            tile->b0 = color;
            tile->g0 = color;
            tile->r0 = color;
            addPrim(&gGpuCurrentOt[1], tile);

            fadeDr         = (DR_TPAGE*)gGpuPrimCursor;
            gGpuPrimCursor = fadeDr + 1;
            setlen(fadeDr, 1);
            fadeDr->code[0] = 0xE1000240;
            addPrim(&gGpuCurrentOt[1], fadeDr);
        }
    }

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);

    dr             = (DR_TPAGE*)gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000600;
    addPrim(&gGpuCurrentOt[0], dr);

    for (i = 0; i < count; i++) {
        p0             = (POLY_FT4*)gGpuPrimCursor;
        p1             = p0 + 1;
        gGpuPrimCursor = p0 + 2;
        setlen(p0, 9);
        setcode(p0, 0x2F);
        setlen(p1, 9);
        setcode(p1, 0x2F);
        p0->x0 = p0->x2 = x - (0xA0 + y);
        p0->x1 = p0->x3 = x;
        p0->y0 = p0->y1 = -0x78 - y;
        p0->y2 = p0->y3 = y + 0x77;
        right           = cx + 0x9F;
        p1->x0 = p1->x2 = x;
        p1->x1 = p1->x3 = right;
        flag            = mode;
        flag2           = flag;
        p1->y0 = p1->y1 = -0x78 - y;
        p1->y2 = p1->y3 = y + 0x77;
        if (flag2) {
            p0->tpage = 0x100;
            p0->u0 = p0->u2 = 0;
            p0->u1 = p0->u3 = 0xA0;
            p0->v0 = p0->v1 = 0;
            p0->v2 = p0->v3 = 0xEF;
            p1->tpage       = 0x102;
            p1->u0 = p1->u2 = 0x20;
            p1->u1 = p1->u3 = 0xBF;
            p1->v0 = p1->v1 = 0;
            p1->v2 = p1->v3 = 0xEF;
        } else {
            p0->tpage = 0x110;
            p0->u0 = p0->u2 = 0;
            p0->u1 = p0->u3 = 0xA0;
            p0->v0 = p0->v1 = 0x10;
            p0->v2 = p0->v3 = 0xFF;
            p1->tpage       = 0x112;
            p1->u0 = p1->u2 = 0x20;
            p1->u1 = p1->u3 = 0xBF;
            p1->v0 = p1->v1 = 0x10;
            p1->v2 = p1->v3 = 0xFF;
        }
        addPrim(&gGpuCurrentOt[0], p0);
        addPrim(&gGpuCurrentOt[0], p1);
    }

    dr             = (DR_TPAGE*)gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000400;
    addPrim(&gGpuCurrentOt[0], dr);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[0x3FF], stp);
}

void Gp_ApplyAreaTmdFlags(void)
{
    Task*          head;
    Task*          iter;
    GpAreaKey*     key;
    GpAreaRec*     rec;
    GpAreaVariant* nested;
    GpAreaTmdRec*  table;
    GpAreaTmdRec*  entry;
    GpWorkObj*     work;
    GpAreaPlace*   place;
    TmdObject*     extra;
    u16            id;
    u16            flags;
    u16            limit;
    u8             idx;

    head = (gameGetPtrSlot(4))->firstChild;
    if (head != NULL) {
        iter = head;
        do {
            work = iter->spawnArg2;
            if (iter->spawnType == 1) {
                key   = (GpAreaKey*)&Mc_SaveData[0].at4.loc.view;
                idx   = key->stage;
                extra = iter->extra.tmd;
                rec   = Gp_AreaTables[idx];
                place = work->field_3C;
                table = NULL;
                if (rec != NULL) {
                    nested = rec[key->area].field_0;
                    if (nested != NULL) {
                        table = nested[key->place].field_4;
                    }
                }
                entry = table;
                id    = entry->field_0;
                if (id != 0xFF) {
                    limit = 0xFF;
                    do {
                        if (id == place->entryId) {
                            flags = entry->field_8->flags;
                            if (flags == 1) {
                                extra->flags &= 0xFFFB;
                            } else if (flags == 0x101) {
                                extra->flags |= 4;
                            }
                            break;
                        }
                        entry++;
                        id = entry->field_0;
                    } while (id != limit);
                }
            }
            iter = iter->nextSibling;
        } while (iter != head);
    }
}

void Gp_ReparentCoord(GpCoord* arg0, GpCoord* arg1)
{
    GpCoord* dest;

    dest = arg1;
    if (dest->sub != arg0) {
        Gp_UpdateCoord(arg0);
        Gp_UpdateCoord(dest);
        dest->sub = arg0;
        Gp_WorldToLocal(&arg0->workm, &dest->workm, &dest->coord);
        dest->flg = 0;
    }
}

GpWorkObj* Gp_FindWorkById(u16 arg0)
{
    Task*      head;
    Task*      iter;
    GpWorkObj* work;
    s32        key;

    work = NULL;
    head = (gameGetPtrSlot(4))->firstChild;
    if (head != NULL) {
        iter = head;
        work = iter->spawnArg2;
        key  = arg0;
        if (work->field_8.as_u16 != key) {
        loop:
            iter = iter->nextSibling;
            work = NULL;
            if (iter != head) {
                work = iter->spawnArg2;
                if (work->field_8.as_u16 != key) {
                    goto loop;
                }
            }
        }
    }
    return work;
}

void Gp_SetTmdBytes(TmdObject* arg0, s32 arg1, s32 arg2)
{
    arg0->tpage = arg1;
    arg0->clut  = arg2;
    if (arg0->buffer != NULL) {
        tmdProcessStream(arg0);
        tmdProcessStream(arg0);
    }
}

static void Gp_SetCurAreaFlag2(s32 arg0)
{
    GpAreaRec* rec;
    GpAreaObj* obj;
    GpAreaKey* key;

    key = (GpAreaKey*)&Mc_SaveData[0].at4.loc.view;
    rec = Gp_AreaTables[key->stage];
    if (rec != NULL) {
        obj = rec[key->area].field_4;
        if (obj != NULL) {
            if (obj->field_0 == key->place) {
                if (arg0 == 0) {
                    obj->field_1 &= 0xFD;
                    return;
                }
                obj->field_1 |= 2;
            }
        }
    }
}

s32 Gp_GetAreaFlag2(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaObj* obj;
    s32        val;

    rec = Gp_AreaTables[arg0->stage];
    if (rec != NULL) {
        obj = rec[arg0->area].field_4;
        if (obj != NULL) {
            val = obj->field_1 & 2;
            return val != 0;
        }
    }
    return 0;
}

static GpAreaObj* Gp_GetAreaObj(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaObj* ret;

    rec = Gp_AreaTables[arg0->stage];
    if (rec == NULL) {
        ret = NULL;
    } else {
        ret = rec[arg0->area].field_4;
    }
    return ret;
}

static void func_800B5A48(GpAreaKey* arg0, GpAreaObj* arg1)
{
    s32       j;
    s32       i;
    McPosRec* recs;

    if (arg1->field_0 == 0) {
        arg1->field_0  = 1;
        arg1->field_1 |= 1;
    }
    if (arg1->field_1 & 1) {
        arg1->field_1 &= 0xFC;
        i              = 0x1F;
        recs           = Mc_SaveData[0].enemyPoses;
        do {
            if ((recs[i].placeKey & 0xFFF) == ((arg0->stage << 8) | arg0->area)) {
                if (i != 0x1F) {
                    for (j = i; j < 0x1F; j++) {
                        recs[j] = recs[j + 1];
                    }
                }
                recs[0x1F].spawnState = 0;
                recs[0x1F].placeKey   = 0;
            }
            i--;
        } while (i >= 0);
    }
}

void Gp_SetAreaObjId(GpAreaKey* arg0, s32 arg1, s32 arg2)
{
    GpAreaRec* rec;
    GpAreaObj* obj;

    rec = Gp_AreaTables[arg0->stage];
    if (rec != NULL) {
        obj = rec[arg0->area].field_4;
        if (obj != NULL) {
            if (arg2 != -1) {
                obj->field_0 = arg1;
                if (arg2 == 0) {
                    obj->field_1 &= 0xFE;
                } else {
                    obj->field_1 |= 1;
                }
                obj->field_1 &= 0xFD;
            } else if (obj->field_0 != arg1) {
                obj->field_0 = arg1;
                obj->field_1 = (obj->field_1 | 1) & 0xFD;
            } else {
                obj->field_1 &= 0xFE;
            }
            func_800B5A48(arg0, obj);
        }
    }
}

void Gp_SetAreaFlag2(s32 arg0, GpAreaKey* arg1)
{
    GpAreaRec* rec;
    GpAreaObj* obj;

    rec = Gp_AreaTables[arg1->stage];
    if (rec != NULL) {
        obj = rec[arg1->area].field_4;
        if (obj != NULL) {
            if (obj->field_0 == arg1->place) {
                if (arg0 == 0) {
                    obj->field_1 &= 0xFD;
                    return;
                }
                obj->field_1 |= 2;
            }
        }
    }
}

static GpAreaTmdRec* Gp_GetNestedAreaObj(GpAreaKey* arg0)
{
    GpAreaRec*     rec;
    GpAreaVariant* nested;
    GpAreaTmdRec*  ret;

    rec = Gp_AreaTables[arg0->stage];
    ret = NULL;
    if (rec != NULL) {
        nested = rec[arg0->area].field_0;
        if (nested != NULL) {
            ret = nested[arg0->place].field_4;
        }
    }
    return ret;
}

GpAreaVariant* Gp_GetNestedAreaRec(GpAreaKey* arg0)
{
    GpAreaRec*     rec;
    GpAreaVariant* ret;

    rec = Gp_AreaTables[arg0->stage];
    ret = NULL;
    if (rec != NULL) {
        ret = rec[arg0->area].field_0;
        if (ret != NULL) {
            ret = &ret[arg0->place];
        }
    }
    return ret;
}

void Gp_SetAreaFlag0(GpAreaKey* arg0)
{
    u32        key;
    GpAreaRec* rec;
    GpAreaObj* obj;

    key = *(u32*)&arg0->view & 0xFFFF0000;
    rec = Gp_AreaTables[arg0->stage];
    if (key != 0x3260000) {
        if (rec != NULL) {
            obj = rec[arg0->area].field_4;
            if (obj != NULL) {
                obj->field_1 |= 1;
            }
        }
    }
}

void func_800B5DB8(Task* arg0)
{
    TaskFunc funcs[2] = { Gp_BindSlot4, func_800B6398 };

    funcs[arg0->state](arg0);
}

s32 Gp_FindChildType9(Task* arg0, Task* arg1, s32 arg2, Task** arg3)
{
    Task* child;
    s32   ret;

    *arg3 = NULL;
    child = arg0->firstChild;
    ret   = -1;
    if (child == NULL) {
        return ret;
    }
    arg1 = child;
    do {
        arg0 = arg1->spawnArg2;
        if (((((GpWorkObj*)arg0)->field_A >> 8) == 9) && (((GpWorkObj*)arg0)->field_8.as_u16 == arg2)) {
            *arg3 = arg1;
            ret   = 0;
            break;
        }
        arg1 = arg1->nextSibling;
    } while (arg1 != child);
    return ret;
}

s32 Gp_FindChildExceptType9(Task* arg0, Task* arg1, s32 arg2, Task** arg3)
{
    Task* child;
    s32   ret;

    *arg3 = NULL;
    child = arg0->firstChild;
    ret   = -1;
    if (child == NULL) {
        return ret;
    }
    arg1 = child;
    do {
        arg0 = arg1->spawnArg2;
        if (((((GpWorkObj*)arg0)->field_A >> 8) != 9) && (((GpWorkObj*)arg0)->field_8.as_u8 == arg2)) {
            *arg3 = arg1;
            ret   = 0;
            break;
        }
        arg1 = arg1->nextSibling;
    } while (arg1 != child);
    return ret;
}

s32 Gp_ExitChildrenType9(Task* arg0)
{
    Task*      child;
    Task*      next;
    GpWorkObj* work;
    u32        type;

    child = arg0->firstChild;
    if (child == NULL) {
        return 0;
    }
    arg0 = child;
    do {
        work = (GpWorkObj*)arg0->spawnArg2;
        type = work->field_A >> 8;
        next = arg0->nextSibling;
        if (type == 9) {
            Task_CallExit(arg0);
        }
        arg0 = next;
    } while (arg0 != child);
    return 0;
}

s32 Gp_SendMsgType9(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*      child;
    Task*      next;
    GpWorkObj* work;
    u32        type;

    child = arg0->firstChild;
    if (child == NULL) {
        return 0;
    }
    arg0 = child;
    do {
        work = (GpWorkObj*)arg0->spawnArg2;
        type = work->field_A >> 8;
        next = arg0->nextSibling;
        if (type == 9) {
            Gp_DispatchMsg(arg0, arg3, arg2, 0);
        }
        arg0 = next;
    } while (arg0 != child);
    return 0;
}

static void Gp_KillSlot4Children(void)
{
    Task_KillChildren(gameGetPtrSlot(4));
}

static void func_800B6014(void)
{
}

void Gp_SyncAreaKeyIndex(GpAreaKey* arg0)
{
    GpAreaRec*     rec;
    GpAreaVariant* rec2;
    GpAreaObj*     obj;

    rec         = Gp_AreaTables[arg0->stage];
    arg0->place = 1;
    if (rec == NULL) {
        return;
    }
    rec2 = rec[arg0->area].field_0;
    obj  = rec[arg0->area].field_4;
    if (rec2 == NULL) {
        return;
    }
    if (obj->field_0 == 0) {
        obj->field_0  = 1;
        obj->field_1 |= 1;
    }
    arg0->place = obj->field_0;
}

static void func_800B6094(Task* task)
{
    if (task->spawnArg1 & 1) {
        task->killCountdown = 0;
    }
    task->state++;
}

void func_800B60C0(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80093A5C;
    sp.funcs[arg0->state](arg0);
}

void Gp_MakeDirOffset(SVECTOR* arg0, GpDirSrc* arg1, SVECTOR* arg2)
{
    GpDirScratch* s;
    SVECTOR*      vec;
    GpCoord*      coord;
    s32           scale;

    s       = SCRATCH_PUSH(GpDirScratch);
    vec     = &s->vec;
    vec->vx = arg1->pos.vx - arg0->vx;
    vec->vy = arg1->pos.vy - arg0->vy;
    vec->vz = arg1->pos.vz - arg0->vz;
    coord   = &gGfxViewCoord;
    scale   = SquareRoot0(Gfx_ApplyMatrixNoSf(vec, vec)) - arg1->field_2;
    if (scale >= 0) {
        scale = -scale;
    }
    VectorNormalSS(&s->vec, &s->vec);
    TransposeMatrix(&coord->workm, &s->mtx);
    gfxLoadRotSv(&s->mtx, &s->vec);
    gte_rtv0();
    gte_stsv(vec);
    gte_lddp(scale);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(arg2);
    SCRATCH_POP(GpDirScratch);
}

void Gp_FreeSlot4TmdBuffers(void)
{
    Task*      child;
    Task*      iter;
    TmdObject* obj;

    child = (gameGetPtrSlot(4))->firstChild;
    if (child != NULL) {
        iter = child;
        do {
            if (iter->spawnType == 1) {
                obj         = iter->extra.tmd;
                obj->flags |= 4;
                Tmd_FreeBuffers(obj);
            }
            iter = iter->nextSibling;
        } while (iter != child);
    }
}

/// The 2-bit state of entry `arg0` in the current stage's `Gp_Bit2Banks` flags.
static inline s32 _gpGetCurBit2Flag(s32 arg0)
{
    u32* p;
    u32  word;
    s32  shift;

    p      = &Gp_Bit2Banks[gGameSession->at4.loc.stage].field_4[arg0 >> 4];
    shift  = (arg0 & 0xF) * 2;
    word   = *p;
    word  &= 3 << shift;
    word >>= shift;
    return word;
}

/// Finds the record in the 0xFFFF-terminated `desc` table whose id is
/// `place->field_2` and spawns that enemy at `place`.
static inline void _gpSpawnPlace(GpEnemyDesc* desc, GpBit2Rec* place)
{
    GpEnemy*   enemy;
    Task*      task;
    TmdObject* extra;
    GpCoord*   coord;
    u16        id;

    id = desc->field_0;
    while (id != 0xFFFF) {
        if (id == place->field_2) {
            enemy = Gp_SpawnEnemyFromTable(&desc->field_4, 0, desc->field_0, NULL);
            if (enemy != NULL) {
                task = enemy->task;
                if (task->spawnType != 0) {
                    extra               = task->extra.tmd;
                    coord               = extra->coords;
                    enemy->placeKey     = place->field_0 | (place->field_4 << 8);
                    enemy->workType     = place->field_2;
                    coord->coord.t[0]   = place->field_8;
                    coord->coord.t[1]   = place->field_A;
                    coord->coord.t[2]   = place->field_C;
                    coord->param.rot.vy = place->field_E;
                    if (coord->param.rot.vy != 0) {
                        Gfx_RotMatrixY(&coord->coord, (s16)place->field_E, 1);
                    }
                    coord->flg = 0;
                }
            }
            return;
        }
        desc++;
        id = desc->field_0;
    }
}

/// Walks `Gp_Bit2Banks[Mc_SaveData[0].at4.loc.area / stage]` for a `GpBit2Rec`
/// whose `field_0` equals `arg0`. If the packed 2-bit flag at
/// `Gp_Bit2Banks[gGameSession->at4.loc.stage].field_4` is non-zero, spawns that
/// placement via `Gp_SpawnEnemyFromTable` (same coord/yaw writeback as `Gp_SpawnPlaces`).

/// Inline form of `Gp_GetRelatedQty`: the most of a related item weapon
/// `item` can hold, from bank `bank`'s table, or 0 for a non-weapon id.
static inline s32 _gpRelatedQty(s32 item, s32 bank)
{
    s32 ret;

    item -= 0x80;
    ret   = 0;
    if ((u32)item < 0x20) {
        if (bank == 0) {
            ret = Gp_RelatedQty0.rows[item].field_0;
        } else {
            ret = Gp_RelatedQty1.rows[item].field_0;
        }
    }
    return ret;
}

/// How much of `item` the rows `scan` selects hold: the stack count for
/// stackable ids (0xA0 and up), otherwise 1 if any row carries it and 0 if not.
static inline s16 _gpScanHeldQty(McItemRec* table, McItemScan* scan, s32 item)
{
    s32 index;
    s32 found;
    s32 i;

    found = 0;
    if (item >= 0xA0) {
        index = scan->firstRow;
        return Gp_FindScanQty(table, scan, &index, item);
    }
    for (i = scan->firstRow; i < scan->firstRow + scan->rowCount; i++) {
        if (table[i].itemId == item) {
            found = 1;
            break;
        }
    }
    return found;
}

/// Printed when an enemy's work block cannot be allocated.
static const char Gp_StrNewEnemyNull[] = "new_enemy ---> NULL\n";

/// Three-entry dispatcher table: `Gp_EnemyWaitStart`, `Gp_EnemyWaitTick`, `Gp_DestroyEnemy`.
static const GpEnemyTaskFuncTable3 Gp_EnemyWaitFuncs = { {
    Gp_EnemyWaitStart,
    Gp_EnemyWaitTick,
    Gp_DestroyEnemy,
} };

static const TaskFuncTable3 Gp_StageLoadStates = { {
    Gp_StartStageLoad,
    Gp_FinishStageLoad,
    Gp_StageLoadState2,
} };

static const VECTOR D_80093A28 = { 0, -100, 0, 0 };

static const TaskFuncTable3 D_80093A38 = { {
    func_800B28E0,
    func_800B1EFC,
    Task_CallExit,
} };

/// "ERROR: ex_pdriver_2\n". The three bytes after the terminator are not zero:
/// the original toolchain left them in the alignment gap.
static const char D_80093A44[24] = "ERROR: ex_pdriver_2\n\0\xB7\xB0\x34";

static const TaskFuncTable3 D_80093A5C = { {
    func_800B6094,
    func_800B51F4,
    Task_CallExit,
} };

GpSlot4MessageEntry Gp_Slot4MsgTable[5] = {
    { 2000, { .find = Gp_FindChildType9 } },
    { 2008, { .find = Gp_FindChildExceptType9 } },
    { 2009, { .exit = Gp_ExitChildrenType9 } },
    { 2010, { .send = Gp_SendMsgType9 } },
    { 0x7FFFFFFF, { .exit = NULL } },
};
GpBit2Bank Gp_Bit2Banks[6] = { { NULL, NULL }, { D_map_akropolis_8017A7FC, D_800733FC }, { D_map_dryfield_8017A564, D_800734D4 }, { D_map_dryfield_full_8017A46C, D_800734D4 }, { D_map_shelter_8017A998, D_8007367C }, { D_map_neo_ark_8017A6EC, D_80073844 } };

void Gp_BindSlot4(Task* task)
{
    Game_SetPtrSlot(task, 4);
    task->msgTable = Gp_Slot4MsgTable;
    task->state++;
}

void func_800B6398(void)
{
    Gp_DrawTargetCursor();
}
