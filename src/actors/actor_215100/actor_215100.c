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

AnimationPackedPose D_actor_215100_8014D048[5] = {
#include "assets/actor_215100_animation_034E4_bank1.inc"
};

AnimationPackedRotation D_actor_215100_8014D084[61] = {
#include "assets/actor_215100_animation_034E4_bank4.inc"
};

AnimationRecord D_actor_215100_8014D178[89] = {
#include "assets/actor_215100_animation_034E4_records.inc"
};

u16 D_actor_215100_8014D2DC[20] = {
#include "assets/actor_215100_animation_034E4_indices.inc"
};

AnimationSet D_actor_215100_8014D304 = {
    D_actor_215100_8014D178,
    D_actor_215100_8014D2DC,
    { NULL, D_actor_215100_8014D048, NULL, NULL, D_actor_215100_8014D084, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_215100_8014D32C[2] = {
#include "assets/actor_215100_animation_03754_bank1.inc"
};

AnimationPackedRotation D_actor_215100_8014D344[26] = {
#include "assets/actor_215100_animation_03754_bank4.inc"
};

AnimationRecord D_actor_215100_8014D3AC[104] = {
#include "assets/actor_215100_animation_03754_records.inc"
};

u16 D_actor_215100_8014D54C[20] = {
#include "assets/actor_215100_animation_03754_indices.inc"
};

AnimationSet D_actor_215100_8014D574 = {
    D_actor_215100_8014D3AC,
    D_actor_215100_8014D54C,
    { NULL, D_actor_215100_8014D32C, NULL, NULL, D_actor_215100_8014D344, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_215100_8014D59C[2] = {
#include "assets/actor_215100_animation_039AC_bank1.inc"
};

AnimationPackedRotation D_actor_215100_8014D5B4[35] = {
#include "assets/actor_215100_animation_039AC_bank4.inc"
};

AnimationRecord D_actor_215100_8014D640[89] = {
#include "assets/actor_215100_animation_039AC_records.inc"
};

u16 D_actor_215100_8014D7A4[20] = {
#include "assets/actor_215100_animation_039AC_indices.inc"
};

AnimationSet D_actor_215100_8014D7CC = {
    D_actor_215100_8014D640,
    D_actor_215100_8014D7A4,
    { NULL, D_actor_215100_8014D59C, NULL, NULL, D_actor_215100_8014D5B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_215100_8014D7F4[2] = {
#include "assets/actor_215100_animation_03B48_bank1.inc"
};

AnimationPackedRotation D_actor_215100_8014D80C[20] = {
#include "assets/actor_215100_animation_03B48_bank4.inc"
};

AnimationRecord D_actor_215100_8014D85C[57] = {
#include "assets/actor_215100_animation_03B48_records.inc"
};

u16 D_actor_215100_8014D940[20] = {
#include "assets/actor_215100_animation_03B48_indices.inc"
};

AnimationSet D_actor_215100_8014D968 = {
    D_actor_215100_8014D85C,
    D_actor_215100_8014D940,
    { NULL, D_actor_215100_8014D7F4, NULL, NULL, D_actor_215100_8014D80C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_215100_8014D990[2] = {
#include "assets/actor_215100_animation_03D98_bank1.inc"
};

AnimationPackedRotation D_actor_215100_8014D9A8[45] = {
#include "assets/actor_215100_animation_03D98_bank4.inc"
};

AnimationRecord D_actor_215100_8014DA5C[77] = {
#include "assets/actor_215100_animation_03D98_records.inc"
};

u16 D_actor_215100_8014DB90[20] = {
#include "assets/actor_215100_animation_03D98_indices.inc"
};

AnimationSet D_actor_215100_8014DBB8 = {
    D_actor_215100_8014DA5C,
    D_actor_215100_8014DB90,
    { NULL, D_actor_215100_8014D990, NULL, NULL, D_actor_215100_8014D9A8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_215100_8014DBE0[2] = {
#include "assets/actor_215100_animation_03FF0_bank1.inc"
};

AnimationPackedRotation D_actor_215100_8014DBF8[46] = {
#include "assets/actor_215100_animation_03FF0_bank4.inc"
};

AnimationRecord D_actor_215100_8014DCB0[78] = {
#include "assets/actor_215100_animation_03FF0_records.inc"
};

u16 D_actor_215100_8014DDE8[20] = {
#include "assets/actor_215100_animation_03FF0_indices.inc"
};

AnimationSet D_actor_215100_8014DE10 = {
    D_actor_215100_8014DCB0,
    D_actor_215100_8014DDE8,
    { NULL, D_actor_215100_8014DBE0, NULL, NULL, D_actor_215100_8014DBF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_215100_8014DE38[6] = {
#include "assets/actor_215100_animation_042F4_bank1.inc"
};

AnimationPackedRotation D_actor_215100_8014DE80[46] = {
#include "assets/actor_215100_animation_042F4_bank4.inc"
};

AnimationRecord D_actor_215100_8014DF38[109] = {
#include "assets/actor_215100_animation_042F4_records.inc"
};

u16 D_actor_215100_8014E0EC[20] = {
#include "assets/actor_215100_animation_042F4_indices.inc"
};

AnimationSet D_actor_215100_8014E114 = {
    D_actor_215100_8014DF38,
    D_actor_215100_8014E0EC,
    { NULL, D_actor_215100_8014DE38, NULL, NULL, D_actor_215100_8014DE80, NULL, NULL, NULL },
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
                if (Gp_GetCapEventKey() != 0) {
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
            if (Gp_CapBusy() == 0) {
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
            if (Gp_CapBusy() == 0) {
                func_mist_shooting_gallery_8017F95C(0);
                task->state++;
            }
            break;
        case 0x16:
            task->state = 0x1E;
            break;
        case 0x1E:
            Gp_PlayerWeaponId(&D_actor_215100_8014CF84.source.index);
            Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, &D_actor_215100_8014CF84, 0);
            Gp_DispatchMsgPtr(Gp_LookupSlot4(1), 0x7D3, &D_actor_215100_8014D010, 0);
            task->killCountdown = 0x1B;
            Gp_StartCapSlot(0xB, 0, 0);
            task->state++;
            break;
        case 0x1F:
            if (task->killCountdown >= -0x3E7) {
                task->killCountdown--;
            }
            if (task->killCountdown == 0) {
                Gp_DispatchMsgPtr(Gp_LookupSlot4(1), 0x7D3, &D_actor_215100_8014D024, 0);
            }
            if (task->killCountdown == -0x16) {
                Gp_DispatchMsgPtr(Gp_LookupSlot4(1), 0x7D3, &D_actor_215100_8014CFAC, 0);
            }
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 0x20:
            D_actor_215100_8015E670.value = Gp_GetCapEventKey();
            func_mist_shooting_gallery_8017DCAC(D_actor_215100_8015E670.value);
            task->state = 0x28;
            break;
        case 0x28:
            Gp_StartCapSlot(Gp_GetCapEventKey() + 0xB, 0, 0);
            func_800E8614(D_actor_215100_8014F138, 1);
            task->state++;
            break;
        case 0x2A:
            task->state = 0x32;
            break;
        case 0x32:
            D_actor_215100_8014D040++;
            Gp_StateC08.field_6 |= 2;
            if (D_actor_215100_8015E670.value < 3) {
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
            Task_SpawnFromTable(D_mist_shooting_gallery_801856B8, 0, D_actor_215100_8015E670.value - 1, 0);
            D_actor_215100_8014D03C = 1;
            taskKill(task);
            break;
    }
}
