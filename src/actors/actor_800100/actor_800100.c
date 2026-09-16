#include "common.h"

#include "main/mem.h"
#include "actors/actor_800100.h"

INCLUDE_RODATA("actors/nonmatchings/actor_800100/actor_800100", D_actor_800100_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80161F20);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80162264);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_801624F0);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80162A14);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80162E90);

void func_actor_800100_801631C8(Task* arg0)
{
    GpObj* temp_a0;
    void*  temp_s1;

    temp_a0 = arg0->idMap;
    temp_s1 = arg0->spawnArg2;
    if (temp_a0 != NULL) {
        Gp_UnlinkObj(temp_a0);
    }
    Gp_ReleaseState1CMem(temp_s1, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_80163214);

INCLUDE_ASM("actors/nonmatchings/actor_800100/actor_800100", func_actor_800100_801635F4);

/// Texture-upload state of the actor: runs two independent sequences, each a
/// countdown (`field_988` / `field_98B`, reloaded with 4 / 8) that advances a
/// frame index (`field_989` / `field_98C`) through the NULL-terminated image
/// list `D_actor_800100_80167200` / `D_actor_800100_80167210` named by the
/// sequence number (`field_987` / `field_98A`), ending the sequence when its
/// list runs out. Every upload posts its image over an 8-byte `RECT` borrowed
/// from `G_SCRATCH_HEAD` and gives it back at the end of the call.
void func_actor_800100_80163A58(GpActorWork* arg0)
{
    void**      scratch;
    u8*         head;
    s32         temp;
    RECT*       rect;
    GameActor*  actor;
    GpImgRec*** table;
    s32         idx;
    u32         row;
    GpImgRec*   img;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    actor    = arg0->actor;
    temp     = (s32)(head - 8);
    *scratch = (void*)temp;
    rect     = (RECT*)temp;

    if ((s8)actor->field_987 != 0) {
        actor->field_988--;
        if ((s8)actor->field_988 <= 0) {
            table = D_actor_800100_80167200;
            idx   = (s8)actor->field_987 - 1;
            img   = table[idx][(s8)actor->field_989];
            if (img != NULL) {
                ((RECT*)head)[-1].x = 0;
                rect->y             = 0x28;
                rect->w             = 0x15;
                rect->h             = 8;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_988 = 4;
                actor->field_989++;
            } else {
                actor->field_987 = 0;
            }
        }
    }

    if ((s8)actor->field_98A != 0) {
        actor->field_98B--;
        if ((s8)actor->field_98B <= 0) {
            table = D_actor_800100_80167210;
            idx   = (row = (s8)actor->field_98A - 1);
            img   = table[row][(s8)actor->field_98C];
            if (img != NULL) {
                rect->x = 8;
                rect->y = 0x40;
                rect->w = 0xD;
                rect->h = 0xC;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_98B = 8;
                actor->field_98C++;
            } else {
                actor->field_98A = 0;
            }
        }
    }

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

void func_actor_800100_80163BF8(Task* arg0)
{
    arg0->state = 3;
}

void func_actor_800100_80163C04(GpActorWork* arg0)
{
    GameActor* actor;
    GpActorD4* d4;
    Task*      task;

    actor            = arg0->actor;
    d4               = actor->field_910;
    Gp_ActorSlots[1] = NULL;
    task             = actor->field_914;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = actor->field_918;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = actor->field_91C;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = actor->field_920;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = actor->field_924;
    if (task != NULL) {
        Task_Kill(task);
    }
    Gp_UnlinkObj((GpObj*)actor->field_AC);
    Gp_UnlinkObj((GpObj*)actor->field_CC);
    Gp_UnlinkObj((GpObj*)actor->field_EC);
    Gp_UnlinkObj((GpObj*)actor->field_10C);
    Gp_UnlinkObj((GpObj*)d4->field_68);
    Task_Kill((Task*)arg0);
}

INCLUDE_RODATA("actors/nonmatchings/actor_800100/actor_800100", ActorsShared801328ccTable);
