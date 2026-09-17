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

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80131EB0);

void func_actor_121300_801326EC(Task* arg0)
{
    Actor121300FadeWork* fade;
    Actor121300FadeWork* alloc;

    fade = (Actor121300FadeWork*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            alloc       = (Actor121300FadeWork*)Mem_Malloc(8, 0);
            arg0->idMap = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
                return;
            }
            fade    = alloc;
            fade->r = 0xFF;
            fade->g = 0xFF;
            fade->b = 0xFF;
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->b, 2);
            goto state_inc;
        case 2:
            SetDispMask(1);
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->b, 2);
        state_inc:
            arg0->state += 1;
            break;
        case 3:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->b, 2);
            fade->r = (s16)((u16)fade->r - (u16)arg0->spawnArg1);
            fade->g = (s16)((u16)fade->g - (u16)arg0->spawnArg1);
            fade->b = (s16)((u16)fade->b - (u16)arg0->spawnArg1);
            if ((s16)fade->r < 0) {
                Task_Kill(arg0);
            }
            break;
    }
}

/// `func_800B4114` is declared locally with a signed `arg2`; see `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Slot re-arm of the cutscene actor: ticks all nineteen animation slots, and
/// once every one of slots 1..18 has `field_10` bit 0x100 set ("finished"),
/// hands them the animation id `D_actor_121300_8013CC18` holds for the current
/// `field_4A0`, blending it in over ten frames.  A negative table entry leaves
/// the slots alone and only the return value follows.  The gotos reproduce
/// retail's block layout.
s32 func_actor_121300_80132818(Task* arg0)
{
    Actor121300Work* work;
    Actor121300Work* ctx;
    u16              i;
    u16              done;
    u16              anim;

    work = (Actor121300Work*)arg0->idMap;
    for (i = 1; i < 0x13; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    i    = 1;
    done = 1;
    for (; i < 0x13; i++) {
        if (!(work->slots[i].field_10 & 0x100)) {
            goto fail;
        }
    }
check:
    if (done) {
        if (D_actor_121300_8013CC18[work->field_4A0] >= 0) {
            anim           = D_actor_121300_8013CC18[work->field_4A0];
            ctx            = (Actor121300Work*)arg0->idMap;
            ctx->field_4A0 = anim;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&ctx->anim, i, anim, 0, 10);
            }
        }
        return 1;
    }
    return 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_121300/actor_121300", D_actor_121300_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013293C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133064);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013322C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013343C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133580);

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
    Actor121300Work* work = (Actor121300Work*)arg0->idMap;

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

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133854);

extern s32   D_actor_121300_8013CC08;
extern s32   D_actor_121300_8013CC88;
extern Task* D_actor_121300_8013D418;

/// First tick of the cutscene actor: allocates the 0x4B0-byte
/// `Actor121300Work` block, zeroes it and parks it in `Task::idMap`, then wires
/// the model object up -- the work block's light and colour matrices into
/// `TmdObject::field_1C` / `field_20`, `field_C` cleared and the animation
/// context handed to `func_800B3F84`, and slots 1..18 re-armed through
/// `Gp_AnimResetSlot`.  The texture page / CLUT row come from the placement
/// record at the nested area table's `field_0` list whose id matches neither
/// 0xFF (end) nor 0x84 (the skip marker).
///
/// The slot loop reaches the work block through `Task::idMap` again rather than
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

    tmd         = arg0->extra;
    coord       = tmd->field_8;
    map         = Mem_Malloc(0x4B0, 0);
    arg0->idMap = map;
    if (map == NULL) {
        Task_Kill(arg0);
        return;
    }
    work = (Actor121300Work*)map;
    Mem_Set(work, 0, 0x4B0);
    work->field_488         = Game_GetPtrSlot(3);
    D_actor_121300_8013D418 = arg0;
    coord->sub              = &Gfx_ViewCoord;
    tmd->field_1C           = &work->field_43C;
    tmd->field_C            = 0;
    tmd->field_20           = &work->field_45C;
    place                   = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&Game_Session->field_4)->field_0;
    id                      = place->field_0;
    while (id != 0xFF) {
        if (id == 0x84) {
            break;
        }
        place++;
        id = place->field_0;
    }
    Gp_SetTmdBytes(tmd, ((s8*)place)[0xD], ((s8*)place)[0xE]);
    work->field_4AC = (s16)(s8)place->field_D;
    func_800B3F84(&work->anim, &D_actor_121300_8013CC08, (GpAnimObj*)tmd, work->field_30C,
                  work->slots);
    slotsWork            = (Actor121300Work*)arg0->idMap;
    slotsWork->field_4A0 = 1;
    i                    = 1;
    do {
        slotsWork->slots[(u16)i].field_9 = 0x10;
        Gp_AnimResetSlot(&slotsWork->anim, (u16)i, 1);
        i++;
    } while ((u16)i < 0x13U);
    arg0->field_24 = &D_actor_121300_8013CC88;
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

extern s32 D_actor_121300_8013CC00;
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
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&scratch.msg, 0);
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
            if (Game_Session->field_1 == 0) {
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
            Mc_SaveData.field_7 = state;
            Mc_SaveData.field_6 = 9;
            Mc_SaveData.field_8 = state;
            D_80071076          = 1;
            Task_Spawn(0, 0x11, 0, 0);
            Task_Kill(arg0);
            return;
    }
    func_actor_121300_80133854(arg0);
    extra          = (TmdObject*)arg0->extra;
    scratch.vec.vx = extra->field_8[1].workm.t[0];
    scratch.vec.vy = ((TmdObject*)arg0->extra)->field_8[1].workm.t[1];
    scratch.vec.vz = ((TmdObject*)arg0->extra)->field_8[1].workm.t[2];
    func_800D7A9C(extra, &scratch.vec, 0, 3);
    D_actor_121300_8013CC00 += 1;
}

void func_actor_121300_8013400C(Task* arg0)
{
    Actor121300FadeWork* fade;
    Actor121300FadeWork* alloc;

    fade = (Actor121300FadeWork*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            alloc       = (Actor121300FadeWork*)Mem_Malloc(8, 0);
            arg0->idMap = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
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
            Task_Kill(arg0);
            break;
    }
}

void func_actor_121300_801340F0(void)
{
    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
}
