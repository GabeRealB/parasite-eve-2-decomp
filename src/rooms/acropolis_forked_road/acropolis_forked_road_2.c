#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "rooms/room_common.h"

/// `rtps`, `rtpt` and `rtv0`. The `inline_c.h` macros of those names assemble
/// to different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// 0x14 work block the forked road's streamed-scene task
/// (`func_acropolis_forked_road_8017DA24`) keeps at `Task::work`
/// (`memCalloc(0x14, 0)` in its state 0).
///
/// `mtx` is `D_80073B8C`, the camera-target matrix the scene walks along
/// `D_acropolis_forked_road_80180F80` once per streamed frame. `target` is the
/// slot-3 task every placement message is addressed to, captured once from
/// `gameGetPtrSlot(3)`. `script` is the scene's script task, which the room
/// task reparents itself under. The return ride
/// (`func_acropolis_forked_road_8017DD60`) also uses `skipper`, the task it
/// spawns when the pad asks to skip the scene, and `skipping`, the flag that
/// says that task is live.
typedef struct AfrStreamWork {
    /* 0x00 */ MATRIX* mtx;
    /* 0x04 */ Task*   target;
    /* 0x08 */ Task*   skipper;
    /* 0x0C */ Task*   script;
    /* 0x10 */ u16     skipping;
    /* 0x12 */ byte    pad_12[0x2];
} AfrStreamWork;
STATIC_ASSERT_SIZEOF(AfrStreamWork, 0x14);

extern TaskDesc D_acropolis_forked_road_80180F44;

/// The camera-target matrix the streamed scene walks along its path table.
extern MATRIX* D_80073B8C;

/// Set to 1 by the fade-out task once the scene has finished.
extern s16 D_80071076;

/// Per-frame path the streamed scene walks `D_80073B8C` along, indexed by
/// `CdCmd_Queue::field_1EA - 1` for the 0x78 frames the ride lasts.
extern SVECTOR D_acropolis_forked_road_80180F80[];

/// The script pair the streamed scene runs.
extern s32 D_acropolis_forked_road_80185058;
extern s32 D_acropolis_forked_road_80185070;

/// The script pair the return ride runs.
extern s32 D_acropolis_forked_road_80185038;
extern s32 D_acropolis_forked_road_80185050;

/// The camera view the room switches to once the return ride is over, and the
/// selector that picks which of the two weapon-id bases the 0x3E8 record uses.
extern u8 D_8007216C;
extern s8 D_8007218A;

/// One byte of gameplay state that field actors read back with `lb`.
extern s8 D_8011540E;

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

extern s32          Gp_LcgState;
extern GpQuadCorner D_80111E38[];

/// The fourteen spawn offsets of the forked road's ambient effects, indexed
/// 0..13 by the first-frame burst below.
extern SVECTOR D_acropolis_forked_road_80182178[14];

/// One bit per in-game day (shifted by `GameSession::at4.loc.view - 1`) for each of
/// the sixteen ambient-effect slots: which of the room's lamps are lit today.
extern u16 D_acropolis_forked_road_801821E8[16];

/// The two points the twin trail is anchored at, relative to its parent frame:
/// `[0]` places the task's own frame and `[1]`, also reached by its own name,
/// the second trail's.
extern SVECTOR D_acropolis_forked_road_80182204[];
extern SVECTOR D_acropolis_forked_road_8018220C;

void func_acropolis_forked_road_8017EC70(GsCOORDINATE2* arg0, s32 arg1, s16 arg2);
void func_acropolis_forked_road_8017F224(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_acropolis_forked_road_8017F650(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_acropolis_forked_road_8017FED4(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
void func_acropolis_forked_road_80180554(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

/// The forked road's streamed-scene task. State 0 allocates the
/// `AfrStreamWork` block, restarts the stream frame counter, cues the stream
/// (slot-6 msg 0xFA4), captures the camera-target matrix and slot 3 in the
/// block and warps slot 3 to the head of the path with a 0x3E9 placement.
/// State 1 sends the same spot again as a 0x3F2. State 2 waits for slot 3 to
/// go idle (msg 0x3F0) and then queues the stream's CD read. State 3 waits for
/// the stream to come up (`CdCmd_Queue::field_1FA`), starts the script pair and
/// reparents this task under it. State 4 drives the ride, moving the camera
/// target to the `field_1EA`th path entry every frame until the pad interrupts
/// it or the path runs out at frame 0x78. State 5 stops the scene, restores
/// the save's room ids, arms the fade-out task and kills this task.
void func_acropolis_forked_road_8017DA24(Task* task)
{
    RoomPlacement  place;
    RoomPlacement  place2;
    u8             slot;
    AfrStreamWork* work;
    AfrStreamWork* blk;
    CdCmdQueue*    queue;

    queue = &CdCmd_Queue;
    work  = (AfrStreamWork*)task->work;
    switch (task->state) {
        case 0:
            blk        = memCalloc(0x14, 0);
            task->work = (TaskIdMap*)blk;
            if (blk == NULL) {
                taskKill(task);
                break;
            }
            queue->field_1EA = 1;
            func_800E9BDC(3, 0x9FF);
            Gp_StateF0.field_4                   = 2;
            ((AfrStreamWork*)task->work)->mtx    = D_80073B8C;
            ((AfrStreamWork*)task->work)->target = gameGetPtrSlot(3);
            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
            place.rot.vy = 0x400;
            place.rot.vx = 0;
            place.rot.vz = 0;
            place.pos.vx = D_acropolis_forked_road_80180F80[0].vx - 0x654;
            place.pos.vy = D_acropolis_forked_road_80180F80[0].vy;
            place.pos.vz = D_acropolis_forked_road_80180F80[0].vz;
            Gp_DispatchMsg(((AfrStreamWork*)task->work)->target, 0x3E9, (s32)&place, 0);
            task->state = task->state + 1;
            break;

        case 1:
            place2.rot.vy = 0x400;
            place2.pos.vx = D_acropolis_forked_road_80180F80[0].vx;
            place2.pos.vy = D_acropolis_forked_road_80180F80[0].vy;
            place2.pos.vz = D_acropolis_forked_road_80180F80[0].vz;
            Gp_DispatchMsg(((AfrStreamWork*)task->work)->target, 0x3F2, (s32)&place2, 0);
            task->state = task->state + 1;
            break;

        case 2:
            if (Gp_DispatchMsg(work->target, 0x3F0, 0, 0) == 0) {
                slot = Stream_FindSlot(&gGameSession->at4.loc.view, 0, 0);
                CdCmd_Enqueue(0x61, 0, &slot);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (queue->field_1FA != 0) {
                work->script                  = Gp_SpawnScript18((s32)&D_acropolis_forked_road_80185058,
                                                                 (s32)&D_acropolis_forked_road_80185070);
                gGameSession->padScriptFlags |= 0x80;
                Task_Reparent(task, work->script);
                task->state = task->state + 1;
            }
            break;

        case 4:
            work->mtx->t[0] = D_acropolis_forked_road_80180F80[queue->field_1EA - 1].vx;
            work->mtx->t[1] = D_acropolis_forked_road_80180F80[queue->field_1EA - 1].vy;
            work->mtx->t[2] = D_acropolis_forked_road_80180F80[queue->field_1EA - 1].vz;
            if ((Pad_CheckFlag800() != 0) || ((queue->field_1EA - 1) >= 0x78)) {
                task->state = task->state + 1;
            }
            break;

        case 5:
            Gp_StateF0.field_4 = 0;
            func_800E9BDC(2, 0x9FF);
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.stage = 1;
            Mc_SaveData.at4.loc.area  = 0xA;
            Mc_SaveData.at4.loc.warp  = 4;
            Mc_SaveData.at4.loc.room  = 1;
            D_80071076                = 1;
            Task_Spawn(0, 0x11, 0, 0);
            gGameSession->padScriptFlags &= 0x7F;
            taskKill(task);
            break;
    }
}

/// The forked road's return ride: the same streamed scene played backwards
/// along `D_acropolis_forked_road_80180F80`, whose entries this one walks from
/// the far end (`0x3B - CdCmd_Queue::field_1EA`).
///
/// State 0 allocates the `AfrStreamWork` block, captures slot 3 and the
/// camera-target matrix (`Player_Status::field_4`) in it, cues the stream
/// (slot-6 msg 0xFA4) and republishes the player's weapon to slot 3 with a
/// 0x3E8 record. State 1 waits for the stream to come up
/// (`CdCmd_Queue::field_1FA`), moves the camera target to the head of the
/// path, starts the script pair, reparents this task under it and blanks the
/// display. State 2 drives the ride: it un-blanks after two frames, walks the
/// camera target along the path, and lets the pad spawn the skip task. Once
/// that task reports done it warps slot 3 to the path's end with a 0x3E9 and
/// arms the ride's exit; otherwise the ride ends on its own when the path runs
/// down to its last 11 entries, which is sent as a 0x3F2. State 3 waits for
/// slot 3 to go idle (msg 0x3F0), releases it (0x3F1), restores the camera
/// view and the session's ride flag and kills this task.
void func_acropolis_forked_road_8017DD60(Task* task)
{
    GpRec14        rec;
    RoomPlacement  place;
    s32            sp40;
    AfrStreamWork* work;
    AfrStreamWork* blk;
    CdCmdQueue*    queue;
    s32            weaponId;

    queue = &CdCmd_Queue;
    work  = (AfrStreamWork*)task->work;
    switch (task->state) {
        case 0:
            blk        = memCalloc(0x14, 0);
            task->work = (TaskIdMap*)blk;
            if (blk == NULL) {
                taskKill(task);
                break;
            }
            ((AfrStreamWork*)task->work)->target = gameGetPtrSlot(3);
            ((AfrStreamWork*)task->work)->mtx    = Player_Status.coordMtx;
            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
            weaponId     = Player_Status.weapon;
            rec.field_0  = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.field_4  = 1;
            rec.field_8  = 0;
            rec.field_C  = 0;
            rec.field_10 = 0;
            Gp_DispatchMsg(((AfrStreamWork*)task->work)->target, 0x3E8, (s32)&rec, 0);
            func_800E9BDC(3, 0x9FF);
            Gp_StateF0.field_4 = 2;
            task->state        = task->state + 1;
            break;

        case 1:
            if (queue->field_1FA != 0) {
                work->mtx->t[0]               = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vx;
                work->mtx->t[1]               = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vy;
                work->mtx->t[2]               = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vz;
                work->script                  = Gp_SpawnScript18((s32)&D_acropolis_forked_road_80185038,
                                                                 (s32)&D_acropolis_forked_road_80185050);
                gGameSession->padScriptFlags |= 0x80;
                Task_Reparent(task, work->script);
                SetDispMask(0);
                task->killCountdown = 0;
                task->state         = task->state + 1;
            }
            break;

        case 2:
            task->killCountdown = task->killCountdown + 1;
            if (task->killCountdown >= 3) {
                SetDispMask(1);
            }
            work->mtx->t[0] = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vx;
            work->mtx->t[1] = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vy;
            work->mtx->t[2] = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vz;
            if (work->skipping != 0) {
                if (Task_PollKill(work->skipper, &sp40) != 0) {
                    place.pos.vx = -0x190;
                    place.pos.vy = 1;
                    place.pos.vz = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vz;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0xC00;
                    Gp_DispatchMsg(((AfrStreamWork*)task->work)->target, 0x3E9, (s32)&place, 0);
                    Task_SpawnFromTable(&D_acropolis_forked_road_80180F44, 4, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (Pad_CheckFlag800() != 0) {
                work->skipper  = Task_SpawnFromTable(&D_acropolis_forked_road_80180F44, 3, 0, 0);
                work->skipping = 1;
            }
            if ((0x3B - queue->field_1EA) < 0xB) {
                place.pos.vx = -0x190;
                place.pos.vy = 1;
                place.pos.vz = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vz;
                Gp_DispatchMsg(((AfrStreamWork*)task->work)->target, 0x3F2, (s32)&place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (Gp_DispatchMsg(work->target, 0x3F0, 0, 0) == 0) {
                Gp_DispatchMsg(work->target, 0x3F1, 0, 0);
                D_8007216C = Gp_FindViewIndex(5);
                Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA5, 0, 0);
                func_800E9BDC(2, 0x9FF);
                Gp_StateF0.field_4            = 0;
                gGameSession->padScriptFlags &= 0x7F;
                taskKill(task);
            }
            break;
    }
}

/// An eight-frame screen fade: draws the fade overlay (mode 2) at the level
/// held in the task's `killCountdown`, which rises by 0x20 a frame, and asks
/// for the task to be killed once it passes 0xFF.
void func_acropolis_forked_road_8017E1C0(Task* arg0)
{
    u8  fade;
    s16 temp_v0;

    fade = (u8)arg0->killCountdown;
    Fade_DrawOverlay(fade, fade, fade, 2);
    temp_v0             = (u16)arg0->killCountdown + 0x20;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x100) {
        Task_RequestKill(arg0, 0);
    }
}

/// The same eight-frame fade run the other way: the overlay level is the
/// complement of the rising counter, so it falls from 0xFF by 0x20 a frame,
/// and the task kills itself once the counter passes 0xFF.
void func_acropolis_forked_road_8017E220(Task* arg0)
{
    u8  fade;
    s16 temp_v0;

    fade = ~(u8)arg0->killCountdown;
    Fade_DrawOverlay(fade, fade, fade, 2);
    temp_v0             = (u16)arg0->killCountdown + 0x20;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}

/// Room script callback: sets `D_8011540E` to 1.
void func_acropolis_forked_road_8017E288(void)
{
    D_8011540E = 1;
}

/// Forked-road ambient effect task. On its first frame it fires one effect per
/// entry of `D_acropolis_forked_road_80182178`, in four runs that differ only
/// in the flavour bits added to the entry's index - two 0x02000000, two
/// 0x03000000, eight 0x02000100 and two 0x00000200 - and then publishes the
/// room's three ambient sound events before marking itself done.
void func_acropolis_forked_road_8017E298(Task* task)
{
    GsCOORDINATE2* coord;
    s32            i;

    coord = ((TmdObject*)task->extra)->coords;
    if (task->state == 0) {
        for (i = 0; i < 2; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x2000000, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 2; i < 4; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x3000000, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 4; i < 0xC; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x2000100, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 0xC; i < 0xE; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x200, &D_acropolis_forked_road_80182178[i]);
        }
        D_80115758  = 0x60290;
        D_8011572C  = 0x60291;
        D_80115750  = 0x60292;
        task->state = task->state + 1;
    }
}

/// Draws one frame of a forked-road wall lamp: a flickering, screen-aligned
/// sprite at the task's own coordinate frame. The lamp is skipped entirely
/// while the effect pool is busy (`Gp_State1C->eventState` at 4 or more) and on
/// the days whose bit is clear in `D_acropolis_forked_road_801821E8`, indexed
/// by the low nibble of `Task::spawnArg1`.
///
/// On the first frame the task unpacks the rest of `spawnArg1` into its
/// effect work block - the half extent into `scale` (bits 16-27, 0x280 when
/// zero), the animation column into `angle` (bits 8-9) and that column's grey
/// level into `period` - and leaves only the day index behind. Every frame it then projects the
/// coordinate's translation through `GsWSMATRIX` with a single `RTPS` into a
/// 0x14-byte `G_SCRATCH_HEAD` block and, for anything at `otz` 0x11 or
/// further, queues one semi-transparent `POLY_FT4` on tpage 0x2B whose
/// half extent is `scale * 39 / otz`, so the lamp shrinks with distance. The
/// grey alternates by 0x10 on the parity of `DisplayState::field_8`, which is
/// what makes it flicker.
void func_acropolis_forked_road_8017E410(Task* task)
{
    void**            scratch;
    RoomShaftScratch* block;
    GpEffWork*        work;
    GsCOORDINATE2*    coord;
    POLY_FT4*         prim;
    DisplayState*     ds;
    s32               rgb;
    s32               flip;
    s16               xy;

    work  = (GpEffWork*)task->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4 &&
        ((D_acropolis_forked_road_801821E8[task->spawnArg1 & 0xF] >> ((u8)gGameSession->at4.loc.view - 1)) & 1)) {
        Gp_UpdateCoord(coord);
        scratch  = (void**)G_SCRATCH_HEAD;
        *scratch = (u8*)*scratch - 0x14;
        block    = (RoomShaftScratch*)*scratch;
        if (task->state == 0) {
            u8 levels[3] = { 0x50, 0x30, 0x10 };

            if (task->spawnArg1 & 0xFFF0000) {
                work->scale = (task->spawnArg1 >> 16) & 0xFFF;
            } else {
                work->scale = 0x280;
            }
            work->angle     = (task->spawnArg1 >> 8) & 3;
            task->spawnArg1 = task->spawnArg1 & 0xF;
            work->period    = levels[work->angle];
            task->state     = task->state + 1;
        }
        block->vec.vx = *(u16*)&coord->workm.t[0];
        block->vec.vy = *(u16*)&coord->workm.t[1];
        block->vec.vz = *(u16*)&coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps_real();
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        gte_stsxy(&block->sx);
        gte_stszotz(&block->otz);
        if (block->otz >= 0x11) {
            ds   = &gDisplayState;
            flip = (u8)ds->animFrame;
            SOFT_BARRIER();
            rgb         = (u8)work->period;
            prim->tpage = 0x2B;
            rgb        += (flip & 1) << 4;
            prim->r0    = rgb;
            prim->g0    = rgb;
            prim->b0    = rgb;
            setSemiTrans(prim, 1);
            setClut(prim, work->angle * 16, 0x10E);
            prim->u0 = work->angle * 0x28;
            prim->v0 = 0;
            prim->u1 = work->angle * 0x28 + 0x27;
            prim->v1 = 0;
            prim->u2 = work->angle * 0x28;
            prim->v2 = 0x27;
            prim->u3 = work->angle * 0x28 + 0x27;
            prim->v3 = 0x27;

            block->halfWidth = (work->scale * 0x27) / block->otz;
            xy               = block->sx - *(u16*)&block->halfWidth;
            prim->x2         = xy;
            prim->x0         = xy;
            xy               = block->sx + *(u16*)&block->halfWidth;
            prim->x3         = xy;
            prim->x1         = xy;
            xy               = block->sy - *(u16*)&block->halfWidth;
            prim->y1         = xy;
            prim->y0         = xy;
            xy               = block->sy + *(u16*)&block->halfWidth;
            prim->y3         = xy;
            prim->y2         = xy;
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    }
}

/// One drifting mote of the room's ambient effect. The first tick seeds it
/// from `Gp_LcgState`: a size of 0x20, a random tilt pair (`field_28` /
/// `field_2A`) and a random drift in `field_10`. While it flies, the drift
/// moves its coordinate frame and the tilt rotates it; each drift axis eases
/// back towards zero by one a tick and re-rolls a fresh multiple of 8 when it
/// gets there, and the tilt wanders by a random step. Once the frame has
/// risen past the origin the mote fades in by 0x10 a tick up to 0x80, then
/// fades back out and releases its work block.
void func_acropolis_forked_road_8017E81C(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    s32            vy;
    s32            vx;
    s32            vz;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    Gp_UpdateCoord(coord);
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24    = 0x20;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1F0);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_2A    = 0x80 - (((u32)Gp_LcgState >> 16) & 0xF0);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            task->state       = 1;
            /* fallthrough */
        case 1:
            coord->coord.t[0] += work->field_10.vx;
            coord->coord.t[1] += work->field_10.vy;
            coord->coord.t[2] += work->field_10.vz;
            Gfx_RotMatrixX(&coord->coord, (s16)work->field_28, 0);
            Gfx_RotMatrixZ(&coord->coord, (s16)work->field_2A, 0);
            coord->flg = 0;

            vy = work->field_10.vy;
            if (vy >= 0x1D) {
                vy = vy - 1;
            } else {
                vy = vy + 1;
            }
            work->field_10.vy = vy;

            vx = work->field_10.vx;
            if (vx == 0) {
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vx += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vx > 0) {
                    vx = vx - 1;
                } else {
                    vx = vx + 1;
                }
                work->field_10.vx = vx;
            }

            vz = work->field_10.vz;
            if (vz == 0) {
                work->field_10.vz += (s16)work->field_2A % 32;
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vz += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vz > 0) {
                    vz = vz - 1;
                } else {
                    vz = vz + 1;
                }
                work->field_10.vz = vz;
            }

            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_28 += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 0x10;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_2A += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 8;

            if (coord->coord.t[1] > 0) {
                task->state = 2;
            }
            func_acropolis_forked_road_8017EC70(coord, (s16)work->field_24, 0);
            break;
        case 2:
            if ((s16)work->field_26 < 0x80) {
                work->field_26 += 0x10;
            } else {
                task->state = 3;
            }
            func_acropolis_forked_road_8017EC70(coord, (s16)work->field_24, 0);
            break;
        case 3:
            if ((s16)work->field_26 >= 0x11) {
                work->field_26 -= 0x10;
                func_acropolis_forked_road_8017EC70(coord, (s16)work->field_24, (s16)work->field_26);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws one mote: the unit quad `D_80111E38` scaled by `arg1`, rotated and
/// placed by the mote's coordinate frame, then projected through
/// `GsWSMATRIX` into a textured quad. A mote nearer than `otz` 0x11 is not
/// drawn. `arg2` is the fade level: zero draws the texture unshaded, anything
/// else modulates it to that grey and draws it semi-transparent.
void func_acropolis_forked_road_8017EC70(GsCOORDINATE2* arg0, s32 arg1, s16 arg2)
{
    register GsCOORDINATE2* coord asm("t7");
    register void**         scratch asm("a0");
    u8*                     head;
    RoomQuadScratch*        blk;
    POLY_FT4*               prim;
    GpQuadCorner*           tbl;
    SVECTOR*                sv;
    MATRIX*                 wm;
    s32                     i;

    coord   = arg0;
    scratch = (void**)G_SCRATCH_HEAD;
    i       = 0;
    wm      = &coord->workm;
    tbl     = D_80111E38;
    head    = (u8*)*scratch - sizeof(RoomQuadScratch);
    /* `head` and `blk` have to stay separate registers: the ROM computes the
       block address into a scratch register and copies it into the one the
       rest of the function uses. */
    SOFT_TOUCH_REG(head);
    blk      = (RoomQuadScratch*)head;
    *scratch = blk;
    do {
        blk->v[i].vx = tbl[i].x * arg1;
        // Spelled as an offset rather than `&blk->v[i]` so it stays a separate
        // pointer from the one the GTE macros below take; writing both the same
        // way lets CSE fold them into one register and the loop stops matching.
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(RoomQuadScratch, v));
        sv->vy = 0;
        sv->vz = tbl[i].y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0_real();
        gte_stsv(&blk->v[i]);
        *(u16*)&blk->v[i].vx = *(u16*)&blk->v[i].vx + *(u16*)&coord->workm.t[0];
        *(u16*)&sv->vy       = *(u16*)&sv->vy + *(u16*)&coord->workm.t[1];
        i++;
        *(u16*)&sv->vz = *(u16*)&sv->vz + *(u16*)&coord->workm.t[2];
    } while (i < 4);

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps_real();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt_real();
    setUV4(prim, 0, 0xE8, 7, 0xE8, 0, 0xEF, 7, 0xEF);
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    if (blk->otz >= 0x11) {
        if (arg2 != 0) {
            setRGB0(prim, arg2, arg2, arg2);
            setSemiTrans(prim, 1);
        } else {
            setShadeTex(prim, 1);
        }
        prim->tpage = 0x2B;
        prim->clut  = 0x4390;
        addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomQuadScratch);
}

/// A flash that swells and then fades. For as many ticks as the spawn argument
/// it brightens and grows two discs and a ring at its frame, all tinted
/// (level, level / 4, level / 2); at full brightness it draws a fade quad, then
/// draws a two-ring billboard that dims by 0x10 a tick and releases its work
/// block once the level falls to 0x10. It pauses while the room's event state
/// is set and releases the block when that state reaches 4.
void func_acropolis_forked_road_8017EF80(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = (u16)work->scale >> 2;
                rgb[2] = (u16)work->scale >> 1;
                func_acropolis_forked_road_8017F650(coord, (s16)work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_acropolis_forked_road_8017F650(coord, (s16)((u16)work->angle * 2), rgb);
                func_acropolis_forked_road_8017F224(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = (u16)work->scale >> 2;
                    rgb[2]      = (u16)work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if ((s16)work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = (u16)work->scale >> 1;
                    func_acropolis_forked_road_80180554(coord, (s16)((s16)work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Queues a gouraud ring of sixteen quads around the projected world position
/// of `arg0`: black at radius `arg1` and shaded `rgb` at radius `arg1 + arg2`,
/// both scaled by depth. Nothing is drawn when the projection overflows.
void func_acropolis_forked_road_8017F224(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    s32                sum;
    s32                otz;
    register s32       rOuter asm("v0");
    s32                rInner;
    u8*                color;
    s32                t;
    u16                vz;
    u16                vx;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    vx = *(u16*)&arg0->workm.t[0];
    USE_REG(vx);
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw02Scratch*)tmp;
    }
    block->vec.vx = vx;
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw02Scratch*)(head - 0x1C))->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw02Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw02Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        USE_REG(head);
        otz                                      = ((RoomDraw02Scratch*)(head - 0x1C))->otz + 1;
        rOuter                                   = ((s16)saved * 64) / otz;
        ((RoomDraw02Scratch*)(head - 0x1C))->otz = otz;
        rInner                                   = (s16)sum * 64;
        block->rOuter                            = rOuter;
        rInner                                   = rInner / ((RoomDraw02Scratch*)(head - 0x1C))->otz;
        ang                                      = 0;
        block->rInner                            = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Queues a gouraud disc of eight wedges around the projected world position
/// of `arg0`, shaded `rgb` at the centre and black at the rim, of radius
/// `arg1` scaled by depth. Nothing is drawn when the projection overflows.
void func_acropolis_forked_road_8017F650(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
{
    RoomDraw04Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    u8*                head;
    s32                otz;
    s32                radius;
    s32                t;
    s32                t2;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw04Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw04Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw04Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw04Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw04Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        otz           = block->otz + 1;
        radius        = ((s16)arg1 * 64) / otz;
        block->otz    = otz;
        block->radius = radius;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// A twin trail. The first tick allocates sixteen coordinate frames, eight for
/// each trail, and seeds them all from the two points offset from the anchor,
/// so both trails start collapsed. Each later tick re-places the two points,
/// records them in the next slot of each ring of eight and draws the trails
/// between the rings as a beam. The work block is released once the tick count
/// reaches the spawn argument. It idles while the room's event state is 2 or
/// more.
void func_acropolis_forked_road_8017F9E4(Task* task)
{
    GsCOORDINATE2  coord;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* objCoord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    SVECTOR*       vec;
    s32            i;

    coords   = (GsCOORDINATE2*)task->work;
    work     = (GpEffWork*)task->spawnArg2;
    objCoord = ((TmdObject*)task->extra)->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GsCOORDINATE2*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_acropolis_forked_road_80182204[0].vx;
                objCoord->coord.t[1] = D_acropolis_forked_road_80182204[0].vy;
                objCoord->coord.t[2] = D_acropolis_forked_road_80182204[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_acropolis_forked_road_80182204[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->parent;
                coord.coord.t[0] = D_acropolis_forked_road_8018220C.vx;
                coord.coord.t[1] = D_acropolis_forked_road_8018220C.vy;
                coord.coord.t[2] = D_acropolis_forked_road_8018220C.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->age & 7) + 8];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_acropolis_forked_road_8017FED4(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the beam between two rings of eight coordinate frames as seven
/// gouraud quads, walking back from slot `arg2`, each quad joining two adjacent
/// slots of both rings and dimmer the older it is. `arg3` packs the colour as
/// three multipliers, at bits 8, 4 and 0. A quad whose projection overflows is
/// skipped.
void func_acropolis_forked_road_8017FED4(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GsCOORDINATE2*     a;
    GsCOORDINATE2*     b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    {
        register u8* tmp asm("v0");

        tmp                     = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomDraw03Scratch);
        blk                     = (RoomDraw03Scratch*)tmp;
        *(void**)G_SCRATCH_HEAD = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        blk->v[2].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = *(u16*)&b->workm.t[0];
        blk->v[3].vy = *(u16*)&b->workm.t[1];
        blk->v[3].vz = *(u16*)&b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps_real();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt_real();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = (POLY_G4*)gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomDraw03Scratch);
}

/// A spark burst. The first tick spawns its flash effect; then, for a non-zero
/// spawn argument, it sprays randomly jittered sparks each tick, and for zero
/// it draws a fixed ring and one widening by 0x30 a tick, both dimming by 0x20
/// a tick. Either way it releases its work block after seven ticks. It pauses
/// while the room's event state is set and releases the block when that state
/// reaches 4.
void func_acropolis_forked_road_801802CC(Task* task)
{
    GsCOORDINATE2* objCoord;
    GpEffWork*     work;
    u8             rgb[4];

    objCoord = ((TmdObject*)task->extra)->coords;
    work     = (GpEffWork*)task->spawnArg2;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1 != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = (u16)work->angle >> 1;
            rgb[2]       = (u16)work->angle >> 2;
            func_acropolis_forked_road_8017F224(objCoord, 0x100, 0x100, rgb);
            func_acropolis_forked_road_8017F224(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Queues a star-shaped glow at the projected world position of `arg0`: a
/// disc of radius `arg1` scaled by depth, shaded half `arg2` at the centre, an
/// inner disc of half that radius at full `arg2`, and four thin rays at right
/// angles, alternately reaching the radius and twice it, all fading to black
/// at the rim. Nothing is drawn when the projection overflows.
void func_acropolis_forked_road_80180554(GsCOORDINATE2* arg0, s16 arg1, u8* arg2)
{
    register RoomBillboardScratch* block asm("s3");
    register POLY_G4*              prim asm("s2");
    register s32                   ang asm("s4");
    register void**                scratch asm("a1");
    register u8*                   head asm("a2");
    s32                            t;
    s32                            t2;
    s32                            u;
    u16                            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                             = *(u16*)&arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomBillboardScratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomBillboardScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomBillboardScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz   += 1;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
