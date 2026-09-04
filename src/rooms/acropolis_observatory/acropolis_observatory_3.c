#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/acropolis_observatory.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on,
/// `D_8007218A` picks which of the two weapon-id bases that record uses, and
/// `D_80071075` / `D_80114C12` gate the scene's setup (the latter is the
/// cutscene/among-us mode flag). `D_8007216D` is the field-actor mode byte the
/// scene switches to 1 when it hands control back.
extern u8 D_80073BA9;
extern u8 D_80071075;
extern u8 D_8007216D;
extern s8 D_8007218A;
extern s8 D_80114C12;

/// One byte of gameplay state shared with the field actors; `Room_Util31` and
/// `func_acropolis_observatory_8017D834` write it, and state 3 of the scene
/// task below waits for it to reach 2.
extern s8 D_8011540A;

/// Payloads the observatory scene task sends: `..._8017FE60` is the record
/// slot-3 msg 0x3F4 takes, and `..._8017FE68` holds one `s16` per `step` -- the
/// `field_4` the follow-up 0x3F4 record is sent with, or a negative value when
/// that step sends nothing.
extern s32 D_acropolis_observatory_8017FE60;
extern s16 D_acropolis_observatory_8017FE68[];

/// Per-view spawn table for the observatory's ambient effect. Entry `i` of
/// `D_acropolis_observatory_8017FEB8` is the bitmask of camera views that want
/// effect `i`, tested against `1 << Gp_GetViewIndex()`; the matching entry of
/// `D_acropolis_observatory_8017FE78` is the offset the effect is spawned at.
extern SVECTOR D_acropolis_observatory_8017FE78[8];
extern u16     D_acropolis_observatory_8017FEB8[8];

/// The observatory's scene task. State 0 allocates the `AobSceneWork` block,
/// captures slot 3 in it and cues the scene with the 0x3F4 record at
/// `D_acropolis_observatory_8017FE60`; it does nothing at all while the
/// cutscene flag `D_80114C12` or `D_80071075` is set. States 1, 2 and 4 just
/// tick, state 3 waits for the shared field-actor byte to reach 2 and arms
/// `Gp_ArmStateF0`, state 5 republishes the player's weapon to slot 3 and puts
/// the session back into field mode, and state 6 releases slot 3 (msg 0x3F1)
/// and kills the task.
///
/// Every state then falls into the same tail: while slot 3 is idle (msg 0x3ED
/// returns 0) the `step`th entry of `D_acropolis_observatory_8017FE68` is sent
/// as a second 0x3F4 record, unless that entry is negative.
void func_acropolis_observatory_8017E19C(Task* task)
{
    GpRec14       rec;
    GpRec14       arg;
    GpRec14*      msg;
    AobSceneWork* work;
    AobSceneWork* tail;
    AobSceneWork* dest;
    AobSceneWork* blk;
    s16*          p;
    u16           entry;
    s32           temp;
    s32           weaponId;
    s32           id;

    work = (AobSceneWork*)task->idMap;
    switch (task->state) {
        case 0:
            if (D_80114C12 == 1 || D_80071075 != 0) {
                return;
            }
            blk         = Mem_Calloc(8, 0);
            temp        = (blk == NULL);
            task->idMap = (TaskIdMap*)blk;
            if (temp) {
                Task_Kill(task);
            } else {
                Mem_Set(blk, 0, 8);
                blk->target = Game_GetPtrSlot(3);
            }
            work = (AobSceneWork*)task->idMap;
            if (work->target != NULL) {
                rec.field_0  = (s32)&D_acropolis_observatory_8017FE60;
                rec.field_4  = 1;
                rec.field_8  = 0;
                rec.field_C  = 0;
                rec.field_10 = 1;
                Gp_DispatchMsg(work->target, 0x3F4, (s32)&rec, 0);
            }
            D_8011540A = 0;
            /* fallthrough */
        case 1:
        case 2:
        case 4:
            task->state = task->state + 1;
            break;
        case 3:
            if (D_8011540A == 2) {
                Gp_ArmStateF0(1);
                task->state = task->state + 1;
            }
            break;
        case 5:
            weaponId     = D_80073BA9;
            id           = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.field_0  = id;
            rec.field_4  = 1;
            rec.field_8  = 0;
            rec.field_C  = 0;
            rec.field_10 = 0;
            Gp_DispatchMsg(work->target, 0x3E8, (s32)&rec, 0);
            D_8007216D             = 1;
            Game_Session->field_5  = 1;
            Game_Session->field_76 = 1;
            Game_Session->field_52 = 1;
            task->state            = task->state + 1;
            break;
        case 6:
            Gp_DispatchMsg(work->target, 0x3F1, 0, 0);
            Task_Kill(task);
            break;
    }

    tail = (AobSceneWork*)task->idMap;
    msg  = &arg;
    if (tail->target != NULL && Gp_DispatchMsg(tail->target, 0x3ED, 0, 0) == 0) {
        p     = &D_acropolis_observatory_8017FE68[tail->step];
        temp  = *p;
        entry = *p;
        if (temp >= 0) {
            dest = (AobSceneWork*)task->idMap;
            if (dest->target != NULL) {
                arg.field_0   = (s32)&D_acropolis_observatory_8017FE60;
                arg.field_4   = entry;
                msg->field_8  = 1;
                msg->field_C  = 0xA;
                msg->field_10 = 1;
                Gp_DispatchMsg(dest->target, 0x3F4, (s32)msg, 0);
            }
        }
    }
}

/// Draws the observatory's lens flare: the model's world position is projected
/// through `GsWSMATRIX` into an `AobFlareScratch` block off `G_SCRATCH_HEAD`,
/// and, when the `rtps` reports no error, the projected point becomes the
/// centre of a semi-transparent `POLY_FT4` on tpage 0x2B. The depth used for
/// both the size and the ordering-table slot is the raw `otz` pulled 0x40
/// towards the camera and clamped to 0x10, so the flare stops growing once it
/// is very close. The CLUT alternates between two palettes on odd and even
/// frames, which is what makes the flare flicker.
void func_acropolis_observatory_8017E424(Task* arg0)
{
    void**           scratch;
    u8*              head;
    AobFlareScratch* blk;
    POLY_FT4*        prim;
    GsCOORDINATE2*   coord;
    void*            mem;
    u16              vz;
    s16              x;
    s16              y;

    coord = ((TmdObject*)arg0->extra)->field_8;
    mem   = arg0->spawnArg2;
    Gp_UpdateCoord(coord);

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    blk         = (AobFlareScratch*)(head - sizeof(AobFlareScratch));
    blk->pos.vx = *(u16*)&coord->workm.t[0];
    blk->pos.vy = *(u16*)&coord->workm.t[1];
    vz          = *(u16*)&coord->workm.t[2];
    *scratch    = blk;
    blk->pos.vz = vz;

    {
        SVECTOR* v = &blk->pos;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(v);
    }
    gte_rtps_real();
    gte_stsxy(&blk->sx);
    gte_stflg(&blk->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&blk->otz);
        blk->otz -= 0x40;
        if (blk->otz < 0x10) {
            blk->otz = 0x10;
        }
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = getClut(0xE0 + (u32)(Display_State.field_8 & 1) * 0x10, 0x10F);
        prim->u0    = 0;
        prim->v0    = 0xA0;
        prim->u1    = 0x1F;
        prim->v1    = 0xA0;
        prim->u2    = 0;
        prim->v2    = 0xBF;
        prim->u3    = 0x1F;
        prim->v3    = 0xBF;
        blk->half   = 0x5D00 / blk->otz;
        x           = blk->sx - (u16)blk->half;
        prim->x2    = x;
        prim->x0    = x;
        x           = blk->sx + (u16)blk->half;
        prim->x3    = x;
        prim->x1    = x;
        y           = blk->sy - (u16)blk->half;
        prim->y1    = y;
        prim->y0    = y;
        y           = blk->sy + (u16)blk->half;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(AobFlareScratch);
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Re-spawns the observatory's ambient effects for the current camera view,
/// one per entry whose view mask contains the active view. Skipped entirely
/// once `Gp_State1C::field_4` has reached 4, i.e. once the room has faded out.
void func_acropolis_observatory_8017E6F8(Task* task)
{
    GsCOORDINATE2* coord;
    s32            mask;
    s32            i;
    SVECTOR*       vec;
    u16*           flags;

    coord = ((TmdObject*)task->extra)->field_8;
    mask  = 1 << Gp_GetViewIndex();
    if (Gp_State1C->field_4 < 4) {
        i     = 0;
        vec   = D_acropolis_observatory_8017FE78;
        flags = D_acropolis_observatory_8017FEB8;
        do {
            if (*flags & mask) {
                Gp_SpawnEff(0x60028, coord, 0, vec);
            }
            vec++;
            i++;
            flags++;
        } while (i < 8);
    }
}
