#include "common.h"

#include "actors/actor_121300.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern u32 D_actor_121300_8013BBE8[];
extern u32 D_actor_121300_8013BFD0[];
extern u32 D_actor_121300_8013C3B8[];
extern u32 D_actor_121300_8013C7A0[];
extern u32 D_actor_121300_8013C9D0[];

/// Texture loader: uploads CLUT/texel blocks into the texture page
/// `Actor121300Work::field_4AC` of the parent task named by `spawnArg2`,
/// picking the images by `spawnArg1`; the two-state variants upload one
/// block per frame before killing the task.
void func_actor_121300_8013322C(Task* arg0)
{
    RECT rect;
    s32  page;

    switch (arg0->spawnArg1) {
        case 0:
            page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
            page <<= 6;
            page  += 0x180;
            rect.x = page;
            rect.y = 0x140;
            rect.w = 0x19;
            rect.h = 0x14;
            LoadImage(&rect, D_actor_121300_8013BBE8);
            page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
            page <<= 6;
            page  += 0x18C;
            rect.x = page;
            rect.y = 0x1A0;
            rect.w = 0xE;
            rect.h = 0x14;
            LoadImage(&rect, D_actor_121300_8013C7A0);
            taskKill(arg0);
            break;
        case 1:
            switch (arg0->state) {
                case 0:
                    page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
                    page <<= 6;
                    page  += 0x180;
                    rect.x = page;
                    rect.y = 0x140;
                    rect.w = 0x19;
                    rect.h = 0x14;
                    LoadImage(&rect, D_actor_121300_8013BFD0);
                    arg0->state++;
                    break;
                case 1:
                    page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
                    page <<= 6;
                    page  += 0x180;
                    rect.x = page;
                    rect.y = 0x140;
                    rect.w = 0x19;
                    rect.h = 0x14;
                    LoadImage(&rect, D_actor_121300_8013C3B8);
                    taskKill(arg0);
                    break;
            }
            break;
        case 2:
            switch (arg0->state) {
                case 0:
                    page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
                    page <<= 6;
                    page  += 0x180;
                    rect.x = page;
                    rect.y = 0x140;
                    rect.w = 0x19;
                    rect.h = 0x14;
                    LoadImage(&rect, D_actor_121300_8013BFD0);
                    arg0->state++;
                    break;
                case 1:
                    page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
                    page <<= 6;
                    page  += 0x180;
                    rect.x = page;
                    rect.y = 0x140;
                    rect.w = 0x19;
                    rect.h = 0x14;
                    LoadImage(&rect, D_actor_121300_8013BBE8);
                    taskKill(arg0);
                    break;
            }
            break;
        case 3:
            break;
        case 4:
        case 5:
            page   = ((Actor121300Work*)((Task*)arg0->spawnArg2)->work)->field_4AC;
            page <<= 6;
            page  += 0x18C;
            rect.x = page;
            rect.y = 0x1A0;
            rect.w = 0xE;
            rect.h = 0x14;
            LoadImage(&rect, D_actor_121300_8013C9D0);
            taskKill(arg0);
            break;
    }
}

/// Effect spawner: on every fourth frame, walks one of the two arena-ring
/// position tables `D_actor_121300_8013CCB8` / `D_actor_121300_8013CD48`
/// (`arg1` non-zero picks the lowered one) and spawns effect 0x601B7 at each
/// entry, jittered along `vx` by up to +/-70 -- two LCG draws, the second only
/// when the first one's bit 16 is set, which is also the sign of the step.
/// The walk stops on the zeroed `SVECTOR` that ends both tables.
void func_actor_121300_8013343C(Task* arg0, s16 arg1)
{
    SVECTOR  pos;
    SVECTOR* pts;
    s16      x;
    s32      flags;
    u32      seed;
    s32      vx;

    if (!(D_actor_121300_8013CC00 & 3)) {
        if (arg1 == 0) {
            pts = D_actor_121300_8013CCB8;
        } else {
            pts = D_actor_121300_8013CD48;
        }
        x = pts->vx;
        if (pts->vx != 0) {
            flags = 0x81202400;
            do {
                seed        = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState = seed;
                vx          = x + (((seed >> 16) & 1) ? ((Gp_LcgState = (seed * 5) + 0x71357911) >> 16) & 7
                                                      : -(((Gp_LcgState = (seed * 5) + 0x71357911) >> 16) & 7)) *
                             10;
                pos.vx = vx;
                pos.vy = pts->vy;
                pos.vz = pts->vz;
                Gp_SpawnEff(0x601B7, NULL, flags, &pos);
                pts++;
                x = pts->vx;
            } while (pts->vx != 0);
        }
    }
}

/// Effect spawner: bumps the `Actor121300Work::field_4A8` falloff every 20
/// calls with `arg1` set, then on every fourth frame spawns effect 0x601B7 at
/// the first `6 - field_4A8` entries of `D_actor_121300_8013CDC8`, jittered
/// along `vx` by up to +/-70 as in `func_actor_121300_8013343C`.
void func_actor_121300_80133580(Task* arg0, s16 arg1)
{
    SVECTOR          pos;
    Actor121300Work* work;
    s16              i;
    u32              seed;
    s32              flags;
    SVECTOR*         tbl;
    s32              vx;

    work = (Actor121300Work*)arg0->work;
    if (arg1 != 0) {
        if (++work->field_4AA >= 20) {
            work->field_4AA = 0;
            work->field_4A8++;
        }
    }
    if (!(D_actor_121300_8013CC00 & 3)) {
        for (i = 0; i < 6 - work->field_4A8; i++) {
            flags       = 0x81202400;
            tbl         = D_actor_121300_8013CDC8;
            seed        = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState = seed;
            vx          = tbl[i].vx + (((seed >> 16) & 1) ? ((Gp_LcgState = (seed * 5) + 0x71357911) >> 16) & 7
                                                          : -(((Gp_LcgState = (seed * 5) + 0x71357911) >> 16) & 7)) *
                                 10;
            pos.vx = vx;
            pos.vy = tbl[i].vy;
            pos.vz = tbl[i].vz;
            Gp_SpawnEff(0x601B7, NULL, flags, &pos);
        }
    }
}

extern void     func_8017F334(s32 arg0);
extern void     func_8017F340(u8 arg0, u8 arg1);
extern TaskDesc ActorsShared80136280Desc;
extern s16      D_actor_121300_8013D41C;

/// Waypoint walker: while the current `Actor121300Work::field_49E` waypoint of
/// `D_actor_121300_8013CC20` is live, counts three frames on it, then retunes
/// the view through `func_8017F340`, bumps the value `func_8017F334` passes on
/// and spawns the `ActorsShared80136280Desc[3]` child seeded with the new
/// waypoint index.
void func_actor_121300_80133730(Task* arg0)
{
    Actor121300Work* work = (Actor121300Work*)arg0->work;

    switch (work->field_4A2) {
        case 0:
            D_actor_121300_8013D41C = 1;
            work->field_4A4         = 0;
            work->field_4A6         = 0;
            work->field_4A2        += 1;
            break;
        case 1:
            if (D_actor_121300_8013CC20[work->field_49E].field_6 != -1) {
                if ((s16)++work->field_4A4 >= 3) {
                    if (work->field_4A6 < 6) {
                        func_8017F340((u8)work->field_4A6, 1);
                    } else if (work->field_4A6 >= 7) {
                        func_8017F340((u8)(work->field_4A6 - 1), 1);
                    }
                    func_8017F334(work->field_4A6 + 1);
                    Task_SpawnFromTable(&ActorsShared80136280Desc, 3, work->field_4A6, 0);
                    work->field_4A4 = 0;
                    work->field_4A6 = (s16)((u16)work->field_4A6 + 1);
                }
            }
            break;
    }
}

void            func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
extern s32      func_actor_121300_80132818(Task* arg0);
extern void     func_8017F438(s32 arg0);
extern TaskDesc D_actor_121300_8013BBCC;
extern s16      D_actor_121300_8013CC04;
extern s32      D_actor_121300_8013CCA0;

static inline void func_actor_121300_PlayAll(Task* arg0, s32 anim)
{
    Actor121300Work* work;
    u16              i;

    work            = (Actor121300Work*)arg0->work;
    work->field_4A0 = anim;
    SCHED_BARRIER();
    for (i = 1; i < 0x13; i++) {
        func_800B4114(&work->anim, i, anim, 0, 10);
    }
}

static inline void func_actor_121300_SetCC04(s32 v)
{
    D_actor_121300_8013CC04 = v;
}

void func_actor_121300_80133854(Task* arg0)
{
    Actor121300Work* work;
    CdCmdQueue*      queue;

    work  = (Actor121300Work*)arg0->work;
    queue = &CdCmd_Queue;
    func_actor_121300_80132818(arg0);
    switch ((u16)work->field_498) {
        case 1:
            Gp_DispatchMsg(work->field_488, 0x3F3, 2, 0);
            Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_121300_8013CCA0, 0);
            gGameSession->viewDirty = 1;
            {
                Actor121300Work* slotsWork;
                s32              i;

                slotsWork            = (Actor121300Work*)arg0->work;
                slotsWork->field_4A0 = 1;
                for (i = 1; (u16)i < 0x13U; i++) {
                    slotsWork->slots[(u16)i].rate = 0x10;
                    Gp_AnimResetSlot(&slotsWork->anim, (u16)i, 1);
                }
            }
            work->field_498 = 0;
            break;
        case 2:
            func_actor_121300_PlayAll(arg0, 2);
            work->field_498 = 0;
            break;
        case 4:
            if ((u16)work->field_49A == 0) {
                func_actor_121300_SetCC04(10);
                work->field_47C = 0x3C;
                work->field_47E = 0x100;
                work->field_48C = Task_SpawnFromTable(&D_actor_121300_8013BBCC, 0, 0, (s32)&work->field_47C);
                work->field_49A++;
            }
        case 3:
            func_actor_121300_80133730(arg0);
            break;
        case 8:
            func_actor_121300_SetCC04(0x1E);
            func_actor_121300_80133730(arg0);
            break;
        case 5:
            work->field_480         = 2;
            queue->field_22A        = 0;
            D_actor_121300_8013D41C = 0;
            work->field_498         = 0;
            break;
        case 6:
            if ((u16)work->field_49A == 0) {
                Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_121300_8013CCA0, 0);
                {
                    Actor121300Work* slotsWork;
                    s32              i;

                    slotsWork            = (Actor121300Work*)arg0->work;
                    slotsWork->field_4A0 = 1;
                    for (i = 1; (u16)i < 0x13U; i++) {
                        slotsWork->slots[(u16)i].rate = 0x10;
                        Gp_AnimResetSlot(&slotsWork->anim, (u16)i, 1);
                    }
                }
                func_8017F438(1);
            }
            func_actor_121300_8013343C(arg0, 0);
            func_actor_121300_80133580(arg0, 0);
            break;
        case 7:
            if ((u16)work->field_49A == 0) {
                func_actor_121300_PlayAll(arg0, 3);
                work->field_49A++;
            }
            func_actor_121300_8013343C(arg0, 0);
            break;
        case 9:
            func_actor_121300_80133580(arg0, 1);
            func_actor_121300_8013343C(arg0, 0);
            break;
        case 10:
            switch ((u16)work->field_49A) {
                case 0:
                    work->field_47C = 8;
                    work->field_47E = 0x100;
                    work->field_48C = Task_SpawnFromTable(&D_actor_121300_8013BBCC, 0, 0, (s32)&work->field_47C);
                    work->field_49C = 0;
                    work->field_49A++;
                    break;
                case 1:
                    if (++work->field_49C >= 8) {
                        work->field_480 = 1;
                        work->field_47C = 8;
                        work->field_498 = 0;
                    }
                    break;
            }
            break;
        case 11:
            func_actor_121300_8013343C(arg0, 1);
            break;
        case 12:
            queue->field_22A = 2;
        case 0:
        default:
            work->field_498 = 0;
            break;
    }
}

extern s32   D_actor_121300_8013CC08;
extern s32   D_actor_121300_8013CC88;
extern Task* D_actor_121300_8013D418;

/// First tick of the cutscene actor: allocates the 0x4B0-byte
/// `Actor121300Work` block, zeroes it and parks it in `Task::work`, then wires
/// the model object up -- the work block's light and colour matrices into
/// `TmdObject::lightMtx` / `field_20`, `field_C` cleared and the animation
/// context handed to `func_800B3F84`, and slots 1..18 re-armed through
/// `Gp_AnimResetSlot`.  The texture page / CLUT row come from the placement
/// record at the nested area table's `field_0` list whose id matches neither
/// 0xFF (end) nor 0x84 (the skip marker).
///
/// The slot loop reaches the work block through `Task::work` again rather than
/// through the pointer the setup above uses: the compiler cannot prove
/// `Gp_AnimResetSlot` leaves the task alone, so it reloads, and the reload must
/// stay a separate local for the reload to land in `$s0` as retail does.
void func_actor_121300_80133BFC(Task* arg0)
{
    Actor121300Work* work;
    Actor121300Work* slotsWork;
    TaskIdMap*       map;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    s32              i;
    u8               id;

    tmd        = arg0->extra;
    coord      = tmd->coords;
    map        = Mem_Malloc(0x4B0, 0);
    arg0->work = map;
    if (map == NULL) {
        taskKill(arg0);
        return;
    }
    work = (Actor121300Work*)map;
    Mem_Set(work, 0, 0x4B0);
    work->field_488         = gameGetPtrSlot(3);
    D_actor_121300_8013D418 = arg0;
    coord->sub              = &gGfxViewCoord;
    tmd->lightMtx           = &work->field_43C;
    tmd->flags              = 0;
    tmd->colorMtx           = &work->field_45C;
    place                   = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
    id                      = place->entryId;
    while (id != 0xFF) {
        if (id == 0x84) {
            break;
        }
        place++;
        id = place->entryId;
    }
    Gp_SetTmdBytes(tmd, ((s8*)place)[0xD], ((s8*)place)[0xE]);
    work->field_4AC = (s16)(s8)place->tpage;
    func_800B3F84(&work->anim, &D_actor_121300_8013CC08, tmd, work->field_30C,
                  work->slots);
    slotsWork            = (Actor121300Work*)arg0->work;
    slotsWork->field_4A0 = 1;
    i                    = 1;
    do {
        slotsWork->slots[(u16)i].rate = 0x10;
        Gp_AnimResetSlot(&slotsWork->anim, (u16)i, 1);
        i++;
    } while ((u16)i < 0x13U);
    arg0->msgTable = &D_actor_121300_8013CC88;
}

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 message 0x3E8 record is keyed on,
/// `D_8007218A` picks which of the two weapon-id bases that record uses, and
/// `D_80071075` / `D_80114C12` (the cutscene mode flag) gate the actor's setup.
extern u8  D_80073BA9;
extern u8  D_80071075;
extern s8  D_8007218A;
extern s8  D_80114C12;
extern s16 D_80071076;

extern s32 D_actor_121300_8013CE08;
extern s32 D_actor_121300_8013D2E8;

void func_actor_121300_80133854(Task* arg0);

/// State machine of the cutscene actor, run once per frame from its slot.
/// State 0 waits until no other cutscene is up -- a `D_80114C12` of 1 or a live
/// `D_80071075` means one is -- and then builds the work block through
/// `func_actor_121300_80133BFC` and arms the player's weapon: the slot-3
/// message 0x3E8 record is `D_80073BA9` plus 1 in the alternate weapon block
/// and plus 0x22 in the base one, with `field_4` 1 and the rest of the frame
/// zero.  State 1 hands the cutscene's two script blocks to `func_800E8634`,
/// state 2 spawns the `ActorsShared80136280Desc[9]` child while the session is
/// still down, and state 3 blanks the display, marks save slot 9 / the state
/// and re-arms the first tick before killing the task.
///
/// States 0, 1 and 2 all leave through the same `Task::state` increment; the
/// compiler cross-jumps the three copies, so it appears once, after state 2's
/// body.  Every path but state 3 also steps the actor through
/// `func_actor_121300_80133854` and hands the model's part-1 translation to
/// `func_800D7A9C`.
void func_actor_121300_80133D98(Task* arg0)
{
    Actor121300Scratch scratch;
    TmdObject*         extra;
    s32                state;
    s32                weaponId;
    s32                anim;

    state = arg0->state;
    switch (state) {
        case 0:
            if ((D_80114C12 != 1) && (D_80071075 == 0)) {
                weaponId             = D_80073BA9;
                anim                 = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                scratch.msg.field_0  = (void*)anim;
                scratch.msg.field_4  = 1;
                scratch.msg.field_8  = 0;
                scratch.msg.field_C  = 0;
                scratch.msg.field_10 = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&scratch.msg, 0);
                func_actor_121300_80133BFC(arg0);
                arg0->state += 1;
                break;
            }
            return;
        case 1:
            func_800E8634((s32)&D_actor_121300_8013CE08, 0, (s32)&D_actor_121300_8013D2E8);
            arg0->state += 1;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                Task_SpawnFromTable(&ActorsShared80136280Desc, 9, 0, 0);
                arg0->state += 1;
            }
            break;
        case 3:
            scratch.rect.x = 0;
            scratch.rect.y = 0;
            scratch.rect.w = 0x140;
            scratch.rect.h = 0xF0;
            ClearImage(&scratch.rect, 0, 0, 0);
            scratch.rect.y = 0x110;
            ClearImage(&scratch.rect, 0, 0, 0);
            Mem_Set(Fs_ImgBuffers, 0, 0x25800);
            SetDispMask(1);
            Mc_SaveData.at4.loc.stage = state;
            Mc_SaveData.at4.loc.area  = 9;
            Mc_SaveData.at4.loc.warp  = state;
            D_80071076                = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            return;
    }
    func_actor_121300_80133854(arg0);
    extra          = (TmdObject*)arg0->extra;
    scratch.vec.vx = extra->coords[1].workm.t[0];
    scratch.vec.vy = ((TmdObject*)arg0->extra)->coords[1].workm.t[1];
    scratch.vec.vz = ((TmdObject*)arg0->extra)->coords[1].workm.t[2];
    func_800D7A9C(extra, &scratch.vec, 0, 3);
    D_actor_121300_8013CC00 += 1;
}

void func_actor_121300_8013400C(Task* arg0)
{
    Actor121300FadeWork* fade;
    Actor121300FadeWork* alloc;

    fade = (Actor121300FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor121300FadeWork*)Mem_Malloc(8, 0);
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
            if ((s16)fade->r < 0x100) {
                return;
            }
            SetDispMask(0);
            taskKill(arg0);
            break;
    }
}

void func_actor_121300_801340F0(void)
{
    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
}
