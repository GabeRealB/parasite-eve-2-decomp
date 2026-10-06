#include "actor_215100_private.h"

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"

#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/mist_shooting_gallery.h"

extern AnimationPlayRequest D_actor_215100_8014CF84;
extern AnimationPlayRequest D_actor_215100_8014CFAC;
extern AnimationPlayRequest D_actor_215100_8014D010;
extern AnimationPlayRequest D_actor_215100_8014D024;
extern s32                  D_actor_215100_8014D040;

TaskDesc D_actor_215100_8014CF6C[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_actor_215100_8014A5C0, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_actor_215100_8014A7C4, { .value = 0 } },
};

AnimationPlayRequest D_actor_215100_8014CF84 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014CF98 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014CFAC = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014CFC0[4] = {
    { { .index = 1 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 21, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 22, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_215100_8014D010 = { { .index = 1 }, 23, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014D024 = { { .index = 1 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

s32 D_actor_215100_8014D038 = 0;

s32 D_actor_215100_8014D03C = 0;

s32 D_actor_215100_8014D040 = 0;

s32 D_actor_215100_8014D044 = 0;

static AnimationPackedPose _gActor215100Animation034E4Bank1[5] = {
#include "assets/actor_215100_animation_034E4_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation034E4Bank4[61] = {
#include "assets/actor_215100_animation_034E4_bank4.inc"
};

static AnimationRecord _gActor215100Animation034E4Records[89] = {
#include "assets/actor_215100_animation_034E4_records.inc"
};

static u16 _gActor215100Animation034E4Indices[20] = {
#include "assets/actor_215100_animation_034E4_indices.inc"
};

AnimationSet gActor215100Animation034E4 = {
    _gActor215100Animation034E4Records,
    _gActor215100Animation034E4Indices,
    { NULL, _gActor215100Animation034E4Bank1, NULL, NULL, _gActor215100Animation034E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation03754Bank1[2] = {
#include "assets/actor_215100_animation_03754_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation03754Bank4[26] = {
#include "assets/actor_215100_animation_03754_bank4.inc"
};

static AnimationRecord _gActor215100Animation03754Records[104] = {
#include "assets/actor_215100_animation_03754_records.inc"
};

static u16 _gActor215100Animation03754Indices[20] = {
#include "assets/actor_215100_animation_03754_indices.inc"
};

AnimationSet gActor215100Animation03754 = {
    _gActor215100Animation03754Records,
    _gActor215100Animation03754Indices,
    { NULL, _gActor215100Animation03754Bank1, NULL, NULL, _gActor215100Animation03754Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation039ACBank1[2] = {
#include "assets/actor_215100_animation_039AC_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation039ACBank4[35] = {
#include "assets/actor_215100_animation_039AC_bank4.inc"
};

static AnimationRecord _gActor215100Animation039ACRecords[89] = {
#include "assets/actor_215100_animation_039AC_records.inc"
};

static u16 _gActor215100Animation039ACIndices[20] = {
#include "assets/actor_215100_animation_039AC_indices.inc"
};

AnimationSet gActor215100Animation039AC = {
    _gActor215100Animation039ACRecords,
    _gActor215100Animation039ACIndices,
    { NULL, _gActor215100Animation039ACBank1, NULL, NULL, _gActor215100Animation039ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation03B48Bank1[2] = {
#include "assets/actor_215100_animation_03B48_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation03B48Bank4[20] = {
#include "assets/actor_215100_animation_03B48_bank4.inc"
};

static AnimationRecord _gActor215100Animation03B48Records[57] = {
#include "assets/actor_215100_animation_03B48_records.inc"
};

static u16 _gActor215100Animation03B48Indices[20] = {
#include "assets/actor_215100_animation_03B48_indices.inc"
};

AnimationSet gActor215100Animation03B48 = {
    _gActor215100Animation03B48Records,
    _gActor215100Animation03B48Indices,
    { NULL, _gActor215100Animation03B48Bank1, NULL, NULL, _gActor215100Animation03B48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation03D98Bank1[2] = {
#include "assets/actor_215100_animation_03D98_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation03D98Bank4[45] = {
#include "assets/actor_215100_animation_03D98_bank4.inc"
};

static AnimationRecord _gActor215100Animation03D98Records[77] = {
#include "assets/actor_215100_animation_03D98_records.inc"
};

static u16 _gActor215100Animation03D98Indices[20] = {
#include "assets/actor_215100_animation_03D98_indices.inc"
};

AnimationSet gActor215100Animation03D98 = {
    _gActor215100Animation03D98Records,
    _gActor215100Animation03D98Indices,
    { NULL, _gActor215100Animation03D98Bank1, NULL, NULL, _gActor215100Animation03D98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation03FF0Bank1[2] = {
#include "assets/actor_215100_animation_03FF0_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation03FF0Bank4[46] = {
#include "assets/actor_215100_animation_03FF0_bank4.inc"
};

static AnimationRecord _gActor215100Animation03FF0Records[78] = {
#include "assets/actor_215100_animation_03FF0_records.inc"
};

static u16 _gActor215100Animation03FF0Indices[20] = {
#include "assets/actor_215100_animation_03FF0_indices.inc"
};

AnimationSet gActor215100Animation03FF0 = {
    _gActor215100Animation03FF0Records,
    _gActor215100Animation03FF0Indices,
    { NULL, _gActor215100Animation03FF0Bank1, NULL, NULL, _gActor215100Animation03FF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation042F4Bank1[6] = {
#include "assets/actor_215100_animation_042F4_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation042F4Bank4[46] = {
#include "assets/actor_215100_animation_042F4_bank4.inc"
};

static AnimationRecord _gActor215100Animation042F4Records[109] = {
#include "assets/actor_215100_animation_042F4_records.inc"
};

static u16 _gActor215100Animation042F4Indices[20] = {
#include "assets/actor_215100_animation_042F4_indices.inc"
};

AnimationSet gActor215100Animation042F4 = {
    _gActor215100Animation042F4Records,
    _gActor215100Animation042F4Indices,
    { NULL, _gActor215100Animation042F4Bank1, NULL, NULL, _gActor215100Animation042F4Bank4, NULL, NULL, NULL },
};

void func_actor_215100_80149F2C(Task* task)
{
    s16 slot;

    switch (task->state) {
        case 0x0:
            if (D_actor_215100_8014D038 != 0) {
                taskKill(task);
                return;
            }
            if (task->spawnArg1.value == 0) {
                func_800E8614(D_actor_215100_8014ED90, 1);
            } else {
                slot = 0x1C;
                if (D_actor_215100_8014D040 == 0) {
                    slot = 0x15;
                }
                Gp_StartCapSlot(slot, 0, 0);
                func_800E8614(D_actor_215100_8014EE68, 1);
            }
            task->state++;
            break;
        case 0x2:
            gGameSession->eventState = 1;
            if (task->spawnArg1.value != 0) {
                if (capGetVariantKey() != 0) {
                    task->state = 5;
                } else {
                    task->state = 4;
                }
            } else {
                task->state = 0xA;
            }
            break;
        case 0x4:
            slot = 8;
            if (D_actor_215100_8014D040 == 0) {
                slot = 0x19;
            }
            Gp_StartCapSlot(slot, 0, 0);
            func_800E8614(D_actor_215100_8014EBE0, 1);
            taskKill(task);
            break;
        case 0x5:
            slot = 7;
            if (D_actor_215100_8014D040 == 0) {
                slot = 0x18;
            }
            Gp_StartCapSlot(slot, 0, 0);
            func_800E8614(D_actor_215100_8014EB98, 1);
            task->state++;
            break;
        case 0x6:
            if (gGameSession->eventState == 0) {
                task->state = 0xA;
            }
            break;
        case 0xA:
            Gp_StartCapSlot(9, 0, 0);
            task->state++;
            break;
        case 0xB:
            if (capIsBusy() == 0) {
                func_mist_shooting_gallery_80180B34(0);
                task->state++;
            }
            break;
        case 0xC:
            task->state = 0x14;
            break;
        case 0x14:
            Gp_StartCapSlot(0xA, 0, 0);
            task->state++;
            break;
        case 0x15:
            if (capIsBusy() == 0) {
                func_mist_shooting_gallery_8017F95C(0);
                task->state++;
            }
            break;
        case 0x16:
            task->state = 0x1E;
            break;
        case 0x1E:
            Gp_PlayerWeaponId(&D_actor_215100_8014CF84.source.index);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &D_actor_215100_8014CF84, 0);
            TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(1), 0x7D3, &D_actor_215100_8014D010, 0);
            task->killCountdown = 0x1B;
            Gp_StartCapSlot(0xB, 0, 0);
            task->state++;
            break;
        case 0x1F:
            if (task->killCountdown >= -0x3E7) {
                task->killCountdown--;
            }
            if (task->killCountdown == 0) {
                TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(1), 0x7D3, &D_actor_215100_8014D024, 0);
            }
            if (task->killCountdown == -0x16) {
                TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(1), 0x7D3, &D_actor_215100_8014CFAC, 0);
            }
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case 0x20:
            D_actor_215100_8015E670 = capGetVariantKey();
            func_mist_shooting_gallery_8017DCAC(D_actor_215100_8015E670);
            task->state = 0x28;
            break;
        case 0x28:
            Gp_StartCapSlot(capGetVariantKey() + 0xB, 0, 0);
            func_800E8614(D_actor_215100_8014F138, 1);
            task->state++;
            break;
        case 0x2A:
            task->state = 0x32;
            break;
        case 0x32:
            D_actor_215100_8014D040++;
            Gp_StateC08.flags |= ATTACHMENT_FLAG_SWAP_LOCK;
            if (D_actor_215100_8015E670 < 3) {
                func_800E8614(D_actor_215100_8014EFA0, 1);
            } else {
                func_800E8614(D_actor_215100_8014F060, 1);
            }
            D_actor_215100_8014D038 = 1;
            task->state++;
            break;
        case 0x1:
        case 0x29:
        case 0x33:
            if (gGameSession->eventState == 0) {
                task->state++;
            }
            break;
        case 0x34:
            gGameSession->flowFlags &= (0xFF ^ GAME_SESSION_FLOW_REEQUIP_WEAPON);
            taskSpawnFromTable(D_mist_shooting_gallery_801856B8, 0, D_actor_215100_8015E670 - 1, 0);
            D_actor_215100_8014D03C = 1;
            taskKill(task);
            break;
    }
}
