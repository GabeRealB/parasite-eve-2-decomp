#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"

#include "main/display.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/dryfield_warehouse.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// Work block of the warehouse's cutscene task, allocated as 0x10 zeroed bytes
/// by `func_dryfield_warehouse_8017E090` and parked in `Task::work`. `owner` is
/// the slot-3 game pointer the task dispatches its messages to; `field_4` is
/// the script step `func_dryfield_warehouse_8017DBB0` runs and `field_6` its
/// sub-step, both set by `func_dryfield_warehouse_8017E3F4`; `field_8` and
/// `field_E` are per-step frame counters; `playerEffActive` records that the
/// script has hidden the player's effects and they are to be restored.
typedef struct DwhWork {
    /* 0x00 */ void* owner;
    /* 0x04 */ u16   field_4;
    /* 0x06 */ u16   field_6;
    /* 0x08 */ u16   field_8;
    /* 0x0A */ byte  pad_A[0x2];
    /* 0x0C */ u16   playerEffActive;
    /* 0x0E */ u16   field_E;
} DwhWork;
STATIC_ASSERT_SIZEOF(DwhWork, 0x10);

/// The screen-fade task the warehouse's script spawns and keeps the handle of.
/// `func_dryfield_warehouse_8017DBB0` stores the task `Task_SpawnFromTable`
/// returns, `func_dryfield_warehouse_8017E308` - that task itself - clears it
/// once the fade has run its course, and `func_dryfield_warehouse_8017DA58`
/// kills whatever handle is still there before fading back in.
extern Task* D_dryfield_warehouse_801821C0;

/// The warehouse's cutscene task, published by its own state 0
/// (`func_dryfield_warehouse_8017E090`) so the script helpers can reach its
/// `DwhWork` block.
extern Task* D_dryfield_warehouse_801821BC;

extern u8  D_80071075;
extern u8  D_80073BA9;
extern s8  D_8007218A;
extern s8  D_8007216D;
extern s8  D_80114C12;
extern s32 D_dryfield_warehouse_8017F848;
extern s32 D_dryfield_warehouse_8017F850;
extern s32 D_dryfield_warehouse_8017F868;
extern s16 D_dryfield_warehouse_801821C4;
extern s32 D_dryfield_warehouse_8017F880;
extern s32 D_dryfield_warehouse_8017FA00;

/// Points in the space of the coordinate drawn under: ring centres, one per
/// circle, for the ring drawer, and prism corners for the prism drawer, which
/// the room's only caller points at `[8..15]`.
extern SVECTOR D_dryfield_warehouse_8017FB2C[];
/// Ring radii, parallel to the centres.
extern s16 D_dryfield_warehouse_8017FBAC[];

/// Message handler of the warehouse's cutscene task. Message 0 re-opens the
/// room: it kills the screen-fade task still on `D_dryfield_warehouse_801821C0`,
/// turns the display back on and, while `DwhWork::playerEffActive` is up, ends
/// the weapon effect and re-sends the player-weapon record. The owner is then
/// handed that same 0x3E8 record -- `GpAnimArg::animBlock.index` is the equipped weapon's
/// animation id, `D_80073BA9` plus 1 or 0x22 depending on `D_8007218A`, with 1
/// and 0 padding it out -- followed by the room's placement as msg 0x3E9.
///
/// The session's weapon id is synced to 2 once, and `D_dryfield_warehouse_801821C4`
/// records whether this handler did that: message 1 mirrors the session's view
/// and object tables back onto that flag. The 1 shared by the record and the
/// flag is one callee-saved value because both outlive the dispatches.
void func_dryfield_warehouse_8017DA58(s32 arg0)
{
    DwhWork*  work;
    GpAnimArg rec;
    s32       weaponId;
    s32       anim;

    switch (arg0) {
        case 0:
            if (D_dryfield_warehouse_801821C0 != 0) {
                taskKill(D_dryfield_warehouse_801821C0);
            }
            SetDispMask(1);
            work = (DwhWork*)D_dryfield_warehouse_801821BC->work;
            if (work->playerEffActive != 0) {
                Gp_SpawnWeaponEff();
                work->playerEffActive = 0;
                Gp_MsgPlayerWeapon(0);
            }
            weaponId            = D_80073BA9;
            anim                = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.animBlock.index = anim;
            rec.field_4         = 1;
            rec.field_8         = 0;
            rec.field_C         = 0;
            rec.field_10        = 1;
            Gp_DispatchMsg((Task*)work->owner, 0x3E8, (s32)&rec, 0);
            Gp_DispatchMsg((Task*)work->owner, 0x3E9, (s32)&D_dryfield_warehouse_8017F868, 0);
            if (Mc_SaveData.at4.loc.room != 2) {
                Mc_SaveData.at4.loc.room      = 2;
                gGameSession->at4.loc.room    = 2;
                D_dryfield_warehouse_801821C4 = 1;
                return;
            }
            D_dryfield_warehouse_801821C4 = 0;
            return;
        case 1:
            if (D_dryfield_warehouse_801821C4 != 0) {
                gGameSession->viewDirty     = arg0;
                gGameSession->roomObjsDirty = arg0;
            }
            return;
    }
}

/// Per-frame script step of the warehouse's cutscene task, dispatched on
/// `DwhWork::field_4` with `field_6` as the sub-step. States 1 and 5 advance a
/// frame counter in `field_E` and play a sound every 60 frames; 3 re-sends the
/// weapon record and spawns five staggered effects until `field_8` reaches 36;
/// 4 and 5 draw a white fade. State 2 spawns entry 1 of the room task table into
/// `D_dryfield_warehouse_801821C0`; it, state 0 and any unknown state reset
/// `field_4` to 0, as does state 3 once its timer runs out.
void func_dryfield_warehouse_8017DBB0(Task* arg0)
{
    DwhWork* work;
    DwhWork* shared;
    DwhWork* cur;
    union {
        GpAnimArg rec;
        SVECTOR   pos;
    } msg;
    s32 weaponId;
    s32 anim;

    work = (DwhWork*)arg0->work;
    switch (work->field_4) {
        case 0:
            break;
        case 1:
            switch (work->field_6) {
                case 0:
                    SetDispMask(1);
                    Task_SpawnFromTable(D_dryfield_warehouse_8017FB08, 2, 8, 0);
                    Gp_KillPlayerEffs();
                    work->playerEffActive = 1;
                    cur                   = (DwhWork*)arg0->work;
                    if (cur->owner != NULL) {
                        msg.rec.animBlock.ptr = &D_dryfield_warehouse_8017F848;
                        msg.rec.field_4       = 1;
                        msg.rec.field_8       = 0;
                        msg.rec.field_C       = 0;
                        msg.rec.field_10      = 0;
                        Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)&msg.rec, 0);
                    }
                    Gp_DispatchMsg((Task*)work->owner, 0x3E9, (s32)&D_dryfield_warehouse_8017F850, 0);
                    work->field_8 = 0;
                    work->field_6++;
                    break;
                case 1:
                    if ((work->field_E % 60) == 0) {
                        SndEvt_EnqueueType6(0x52070003, 0, 0);
                    }
                    break;
            }
            work->field_E++;
            return;
        case 2:
            D_dryfield_warehouse_801821C0 = Task_SpawnFromTable(D_dryfield_warehouse_8017FB08, 1, 8, 0);
            break;
        case 3:
            shared = (DwhWork*)D_dryfield_warehouse_801821BC->work;
            if (shared->playerEffActive != 0) {
                Gp_SpawnWeaponEff();
                shared->playerEffActive = 0;
                Gp_MsgPlayerWeapon(0);
            }
            weaponId                = D_80073BA9;
            anim                    = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.rec.animBlock.index = anim;
            msg.rec.field_4         = 1;
            msg.rec.field_8         = 0;
            msg.rec.field_C         = 0;
            msg.rec.field_10        = 1;
            Gp_DispatchMsg((Task*)shared->owner, 0x3E8, (s32)&msg.rec, 0);
            Gp_DispatchMsg((Task*)shared->owner, 0x3E9, (s32)&D_dryfield_warehouse_8017F868, 0);
            switch (work->field_6) {
                case 0:
                    Task_SpawnFromTable(D_dryfield_warehouse_8017FB08, 2, 8, 0);
                    work->field_8 = 0;
                    work->field_6++;
                    return;
                case 1:
                    work->field_8++;
                    shared     = (DwhWork*)arg0->work;
                    msg.pos.vx = 0x1644;
                    msg.pos.vy = 0;
                    if (!(shared->field_8 & 7)) {
                        msg.pos.vz = -500;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((shared->field_8 + 1) & 7)) {
                        msg.pos.vz = -700;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((shared->field_8 + 2) & 7)) {
                        msg.pos.vz = -900;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((shared->field_8 + 3) & 7)) {
                        msg.pos.vz = -1100;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((shared->field_8 + 4) & 7)) {
                        msg.pos.vz = -1300;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (work->field_8 >= 36) {
                        work->field_4 = 0;
                    }
                    SetDispMask(1);
                    return;
            }
            break;
        case 4:
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            switch (work->field_6) {
                case 0:
                    D_8007216D                 = 2;
                    gGameSession->at4.loc.room = 2;
                    work->field_8              = 0;
                    work->field_6++;
                    break;
                case 1:
                    gGameSession->viewDirty     = 1;
                    gGameSession->roomObjsDirty = 1;
                    work->field_6++;
                    break;
                case 2:
                    break;
            }
            if (work->field_8 == 10) {
                SndEvt_EnqueueType6(0x52070004, 0, 0);
            }
            work->field_8++;
            return;
        case 5:
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            switch (work->field_6) {
                case 0:
                    D_80115768    = 0;
                    work->field_E = 0;
                    work->field_6++;
                    break;
                case 1:
                    if ((work->field_E % 60) == 0) {
                        SndEvt_EnqueueType6(0x52070003, 0, 0);
                    }
                    break;
            }
            work->field_E++;
            return;
    }
    work->field_4 = 0;
}

/// Main loop of the warehouse's cutscene task, the owner of the 0x10-byte
/// `DwhWork` block. State 0 arms the script once: a `D_80114C12` of 1 or a live
/// `D_80071075` both mean the cutscene is already up, so it does nothing.
/// Otherwise it parks the zeroed work block in `Task::work` -- a failed
/// `Mem_Malloc` kills the task, but the record below is dispatched either way --
/// fills `owner` from pointer slot 3 and republishes this task as
/// `D_dryfield_warehouse_801821BC` so the room's script helpers reach that block.
///
/// The 0x3E8 record is rebuilt here rather than taken from its owner: `GpAnimArg`
/// field 0 is the equipped weapon's animation id, `D_80073BA9` plus 1 or 0x22
/// depending on `D_8007218A`, and 1 and 0 pad it out. It is dispatched to a
/// freshly fetched slot 3, not to the work block's owner.
///
/// State 0 then falls into state 1, which only steps the machine, so a task
/// entering at 1 runs the step alone. State 2 kills the task once the session
/// has torn down (`gGameSession->eventState`), otherwise runs the script.
void func_dryfield_warehouse_8017E090(Task* arg0)
{
    DwhWork*  work;
    GpAnimArg rec;
    s32       weaponId;
    s32       anim;

    switch (arg0->state) {
        case 0:
            if ((D_80114C12 != 1) && (D_80071075 == 0)) {
                work       = Mem_Malloc(0x10, false);
                arg0->work = (TaskIdMap*)work;
                if (work == NULL) {
                    taskKill(arg0);
                } else {
                    Mem_Set(work, 0, 0x10);
                    work->owner                   = gameGetPtrSlot(3);
                    D_dryfield_warehouse_801821BC = arg0;
                }
                weaponId            = D_80073BA9;
                anim                = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                rec.animBlock.index = anim;
                rec.field_4         = 1;
                rec.field_8         = 0;
                rec.field_C         = 0;
                rec.field_10        = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&rec, 0);
                D_dryfield_warehouse_801821C0 = NULL;
                D_80115768                    = 1;
                arg0->state                   = arg0->state + 1;
                case 1:
                    func_800E8634((s32)&D_dryfield_warehouse_8017F880, 0,
                                  (s32)&D_dryfield_warehouse_8017FA00);
                    arg0->state = arg0->state + 1;
                    return;
            }
            return;
        case 2:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(arg0, 0);
                return;
            }
            func_dryfield_warehouse_8017DBB0(arg0);
            break;
    }
}

/// Screen-fade task running the other way from `func_dryfield_warehouse_8017E308`:
/// on its first tick it allocates the 8-byte block and saturates all three
/// channels at 0xFF, then every frame it draws the fade overlay and lowers each
/// channel by `Task::spawnArg1`. Once `r` falls below 0 the screen is clear and
/// the task kills itself.
void func_dryfield_warehouse_8017E22C(Task* arg0)
{
    OverlayFadeWork* fade;
    OverlayFadeWork* alloc;

    fade = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r = (s16)((u16)fade->r - (u16)arg0->spawnArg1);
            fade->g = (s16)((u16)fade->g - (u16)arg0->spawnArg1);
            fade->b = (s16)((u16)fade->b - (u16)arg0->spawnArg1);
            if (fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

/// Screen-fade task: on its first tick it allocates the 8-byte `r`/`g`/`b`
/// block and seeds all three channels to 0, then every frame it draws the fade
/// overlay and steps each channel up by `Task::spawnArg1`. `r` is the one the
/// end-of-fade test watches, so once it has reached 0x100 the display is
/// switched back on, the room's fade-task handle is cleared and the task kills
/// itself.
void func_dryfield_warehouse_8017E308(Task* arg0)
{
    OverlayFadeWork* fade;
    OverlayFadeWork* alloc;

    fade = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r = (s16)((u16)fade->r + (u16)arg0->spawnArg1);
            fade->g = (s16)((u16)fade->g + (u16)arg0->spawnArg1);
            fade->b = (s16)((u16)fade->b + (u16)arg0->spawnArg1);
            if (fade->r < 0x100) {
                return;
            }
            SetDispMask(0);
            D_dryfield_warehouse_801821C0 = NULL;
            taskKill(arg0);
            break;
    }
}

/// Hands the warehouse's cutscene task the script step `arg0` to run,
/// starting from its first sub-step.
void func_dryfield_warehouse_8017E3F4(s16 arg0)
{
    DwhWork* work = (DwhWork*)D_dryfield_warehouse_801821BC->work;

    work->field_4 = arg0;
    work->field_6 = 0;
}

/// Draws one prism from `D_dryfield_warehouse_8017FB2C[arg1..]` as five gouraud
/// `POLY_G4`: four sides joining the lit ring `[0..3]` to the far ring `[4..7]`,
/// then a cap over the lit ring. Each corner is rotated by `coord`'s `workm` and
/// moved by its translation before projection through `GsWSMATRIX`. The lit
/// corners share a grey of 0x18 plus a small pulse; the far corners are black.
void func_dryfield_warehouse_8017E414(GpCoord* coord, s16 arg1)
{
    RoomQuadScratch* blk;
    POLY_G4*         prim;
    s32              i;
    s32              next;
    s32              far;
    s32              farNext;
    u8               shade;

    SCRATCH_PUSH(RoomQuadScratch);
    blk = SCRATCH_HEAD(RoomQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    shade = (rsin(gDisplayState.animFrame << 10) >> 11) + 0x18;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + i]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx = *(u16*)&blk->v[0].vx + *(u16*)&coord->workm.t[0];
        blk->v[0].vy = *(u16*)&blk->v[0].vy + *(u16*)&coord->workm.t[1];
        blk->v[0].vz = *(u16*)&blk->v[0].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + next]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx = *(u16*)&blk->v[1].vx + *(u16*)&coord->workm.t[0];
        blk->v[1].vy = *(u16*)&blk->v[1].vy + *(u16*)&coord->workm.t[1];
        blk->v[1].vz = *(u16*)&blk->v[1].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + far]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx = *(u16*)&blk->v[2].vx + *(u16*)&coord->workm.t[0];
        blk->v[2].vy = *(u16*)&blk->v[2].vy + *(u16*)&coord->workm.t[1];
        blk->v[2].vz = *(u16*)&blk->v[2].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + farNext]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx = *(u16*)&blk->v[3].vx + *(u16*)&coord->workm.t[0];
        blk->v[3].vy = *(u16*)&blk->v[3].vy + *(u16*)&coord->workm.t[1];
        blk->v[3].vz = *(u16*)&blk->v[3].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        setRGB0(prim, shade, shade, shade);
        setRGB1(prim, shade, shade, shade);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1]);
    gte_rtv0();
    gte_stsv(&blk->v[0]);
    blk->v[0].vx = *(u16*)&blk->v[0].vx + *(u16*)&coord->workm.t[0];
    blk->v[0].vy = *(u16*)&blk->v[0].vy + *(u16*)&coord->workm.t[1];
    blk->v[0].vz = *(u16*)&blk->v[0].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + 1]);
    gte_rtv0();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx = *(u16*)&blk->v[1].vx + *(u16*)&coord->workm.t[0];
    blk->v[1].vy = *(u16*)&blk->v[1].vy + *(u16*)&coord->workm.t[1];
    blk->v[1].vz = *(u16*)&blk->v[1].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + 3]);
    gte_rtv0();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx = *(u16*)&blk->v[2].vx + *(u16*)&coord->workm.t[0];
    blk->v[2].vy = *(u16*)&blk->v[2].vy + *(u16*)&coord->workm.t[1];
    blk->v[2].vz = *(u16*)&blk->v[2].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + 2]);
    gte_rtv0();
    gte_stsv(&blk->v[3]);
    blk->v[3].vx = *(u16*)&blk->v[3].vx + *(u16*)&coord->workm.t[0];
    blk->v[3].vy = *(u16*)&blk->v[3].vy + *(u16*)&coord->workm.t[1];
    blk->v[3].vz = *(u16*)&blk->v[3].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    setRGB0(prim, shade, shade, shade);
    setRGB1(prim, shade, shade, shade);
    setRGB2(prim, shade, shade, shade);
    setRGB3(prim, shade, shade, shade);
    addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    SCRATCH_POP(RoomQuadScratch);
}

/// Draws one ring of gouraud `POLY_G4` segments between two circles in the XZ
/// plane of `coord`: circle `arg1` of the room's centre/radius tables forms the
/// lit edge and circle `arg1 + 1` the black one. `arg2` segments cover the full
/// turn, starting at a phase that advances with the frame counter. Each corner
/// is placed in `coord`'s space through its `workm`, then projected through
/// `GsWSMATRIX`; the lit edge glows at 0x14 plus a small pulse.
void func_dryfield_warehouse_8017ED34(GpCoord* coord, s16 arg1, s16 arg2)
{
    RoomQuadScratch* blk;
    POLY_G4*         prim;
    s16              level;
    s16              step;
    s16              start;
    s32              angle;
    s32              next;

    level = (rsin(gDisplayState.animFrame << 10) >> 11) + 0x14;
    SCRATCH_PUSH(RoomQuadScratch);
    blk   = SCRATCH_HEAD(RoomQuadScratch);
    start = gDisplayState.animFrame & 0xFFF;
    step  = 0x1000 / arg2;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (angle = start; angle < start + step * arg2; angle = next) {
        blk->v[0].vx = D_dryfield_warehouse_8017FB2C[arg1].vx + ((rsin(angle) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        blk->v[0].vy = D_dryfield_warehouse_8017FB2C[arg1].vy;
        blk->v[0].vz = D_dryfield_warehouse_8017FB2C[arg1].vz + ((rcos(angle) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[0]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx += coord->workm.t[0];
        blk->v[0].vy += coord->workm.t[1];
        next          = angle + step;
        blk->v[0].vz += coord->workm.t[2];

        blk->v[1].vx = D_dryfield_warehouse_8017FB2C[arg1].vx + ((rsin(next) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        blk->v[1].vy = D_dryfield_warehouse_8017FB2C[arg1].vy;
        blk->v[1].vz = D_dryfield_warehouse_8017FB2C[arg1].vz + ((rcos(next) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[1]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx += coord->workm.t[0];
        blk->v[1].vy += coord->workm.t[1];
        blk->v[1].vz += coord->workm.t[2];

        blk->v[2].vx = D_dryfield_warehouse_8017FB2C[arg1 + 1].vx + ((rsin(angle) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        blk->v[2].vy = D_dryfield_warehouse_8017FB2C[arg1 + 1].vy;
        blk->v[2].vz = D_dryfield_warehouse_8017FB2C[arg1 + 1].vz + ((rcos(angle) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[2]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx += coord->workm.t[0];
        blk->v[2].vy += coord->workm.t[1];
        blk->v[2].vz += coord->workm.t[2];

        blk->v[3].vx = D_dryfield_warehouse_8017FB2C[arg1 + 1].vx + ((rsin(next) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        blk->v[3].vy = D_dryfield_warehouse_8017FB2C[arg1 + 1].vy;
        blk->v[3].vz = D_dryfield_warehouse_8017FB2C[arg1 + 1].vz + ((rcos(next) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[3]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx += coord->workm.t[0];
        blk->v[3].vy += coord->workm.t[1];
        blk->v[3].vz += coord->workm.t[2];

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        setRGB0(prim, level, level, level);
        setRGB1(prim, level, level, level);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    SCRATCH_POP(RoomQuadScratch);
}

/// Per-frame effect on the room's model task: re-poses the model for the
/// current stage visit, then publishes variant 2 as the room's
/// `Gp_State1C->roomEffectMode` index. `Task::extra` is the task's `TmdObject`, so
/// `field_8` is the coordinate every pose shares. The stage-visit byte
/// `gGameSession->at4.loc.view` is used as a bit index: bits 2, 3, 6 and 9 (`0x24C`)
/// pose through `func_dryfield_warehouse_8017E414`, bit 2 (`4`) also drives
/// `func_dryfield_warehouse_8017ED34` to step 0, those same `0x24C` visits also
/// drive it to step 2, and bits 2, 3, 4 and 6-9 (`0x3DC`) drive it to steps 4
/// and 6.
void func_dryfield_warehouse_8017F494(Task* arg0)
{
    s32      mask;
    s32      poseMask;
    GpCoord* coord;

    mask     = 1 << gGameSession->at4.loc.view;
    poseMask = mask & 0x24C;
    coord    = arg0->extra.tmd->coords;
    if (poseMask != 0) {
        func_dryfield_warehouse_8017E414(coord, 8);
    }
    if (mask & 4) {
        func_dryfield_warehouse_8017ED34(coord, 0, 8);
    }
    if (poseMask != 0) {
        func_dryfield_warehouse_8017ED34(coord, 2, 8);
    }
    if (mask & 0x3DC) {
        func_dryfield_warehouse_8017ED34(coord, 4, 8);
        func_dryfield_warehouse_8017ED34(coord, 6, 8);
    }
    Gp_State1C->roomEffectMode = 2;
}
