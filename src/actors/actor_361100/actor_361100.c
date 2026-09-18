#include "common.h"

#include "actors/actor_361100.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

extern u8 D_801156F9;

extern Task* D_actor_361100_80171BE0;

extern s32 D_8005C374;
extern s32 D_8006D868;
extern s32 D_8007107C;
extern s32 D_8016069C;

void func_80138C9C(Actor361100EffectState* state);
void func_801353D0(Actor361100EffectState* state, GsCOORDINATE2* coord);

void func_actor_361100_80161FF8(Task* arg0);

/// Runs while `D_8006D868` reports a streaming write in flight -- it is `-1`
/// until `Fs_LoadFile` has a chunk, and the mode byte in `gGameSession->field_4`
/// then picks this actor's part in the load: 11 hands the task to
/// `func_actor_361100_80161FF8`, 12 publishes the stream position `D_8016069C`
/// (half the remaining 0x18000-byte window past the write pointer, times the
/// per-chunk rate) and uploads the coordinate, and 10 exits the task.
///
/// State 0 allocates the `Actor361100EffectState` trail block into
/// `Task::work`, seeds its `field_8E` / `field_E0` halfwords and ticks it 0x1E
/// times, then resets the actor's root matrix to identity with the fixed
/// translation (0x1CA2, 0x712, 0x189C) and parks the view coordinate in its
/// `sub` slot. A failed allocation takes the exit call and is *not* branched
/// around: the block pointer is NULL for the rest of the state, as it was in
/// the original.
void func_actor_361100_80161E3C(Task* arg0)
{
    Actor361100EffectState* state;
    GsCOORDINATE2*          coord;
    MATRIX*                 mtx;
    s32                     i;
    s32                     writePtr;
    u32                     streamLeft;
    u8*                     modePtr;
    u8                      mode;

    state   = (Actor361100EffectState*)arg0->work;
    modePtr = &gGameSession->field_4;
    coord   = ((TmdObject*)arg0->extra)->field_8;
    if (D_8006D868 != -1) {
        streamLeft  = 0x18000 - D_8006D868;
        streamLeft &= ~7;
        writePtr    = D_8005C374 + D_8006D868;
        if (arg0->state == 0) {
            state = Mem_Calloc(sizeof(Actor361100EffectState), false);
            if (state == NULL) {
                Task_CallExit(arg0);
                i = 0;
            }
            arg0->work      = (TaskIdMap*)state;
            state->field_E0 = 1;
            state->field_8E = 1;
            i               = 0;
            do {
                func_80138C9C(state);
                i += 1;
            } while (i < 0x1E);
            coord->sub                   = &Gfx_ViewCoord;
            mtx                          = &coord->coord;
            *(s32*)&coord->coord.m[0][0] = 0x1000;
            *(s32*)&mtx->m[0][2]         = 0;
            *(s32*)&mtx->m[1][1]         = 0x1000;
            *(s32*)&mtx->m[2][0]         = 0;
            mtx->m[2][2]                 = 0x1000;
            coord->coord.t[0]            = 0x1CA2;
            coord->coord.t[1]            = 0x712;
            coord->coord.t[2]            = 0x189C;
            coord->flg                   = 0;
            arg0->state                 += 1;
        }
        mode = *modePtr;
        if (mode == 11) {
            func_actor_361100_80161FF8(arg0);
            return;
        } else if (mode == 12) {
            D_8016069C = writePtr + (D_8007107C * ((s32)(streamLeft + (streamLeft >> 0x1F)) >> 1));
            func_80138C9C(state);
            func_801353D0(state, coord);
            return;
        } else if (mode == 10) {
            Task_CallExit(arg0);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100", func_actor_361100_80161FF8);

void func_actor_361100_801627D4(Task* task)
{
    Task*               looker;
    Task*               target;
    Actor361100HeadAim* aim;
    u16                 rate;

    looker = Game_GetPtrSlot(3);
    target = (Task*)Gp_LookupSlot4(2);
    if (D_801156F9 == 0) {
        if ((looker == NULL) || (target == NULL)) {
            task->state = -1;
        }
        switch (task->state) {
            case 0:
                aim = Mem_Calloc(sizeof(Actor361100HeadAim), false);
                if (aim != NULL) {
                    task->work      = (TaskIdMap*)aim;
                    aim->yawLimit   = 0x300;
                    aim->pitchLimit = 0x200;
                    task->state++;
                        /* fallthrough */
                    case 1:
                        aim = (Actor361100HeadAim*)task->work;
                        if (task->spawnArg1 != 0) {
                            rate      = aim->rate + 0x100;
                            aim->rate = rate;
                            if ((s16)rate >= 0x1001) {
                                aim->rate = 0x1000;
                            }
                        } else {
                            rate      = aim->rate - 0x100;
                            aim->rate = rate;
                            if ((s16)rate < 0) {
                                aim->rate = 0;
                            }
                        }
                        func_800B17D4(looker, target, (GpHeadAim*)aim);
                        return;
                }
                /* fallthrough */
            default:
                Task_Kill(task);
                D_actor_361100_80171BE0 = NULL;
                break;
        }
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_361100/actor_361100", D_actor_361100_80161E20);
