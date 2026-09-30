#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

/// The actor's work block, hung off `Task::work`. `func_actor_120500_801322A0`
/// allocates it with `Mem_Malloc(0x4CC, 0)` and zeroes it with `Mem_Set`.
///
/// It opens with the animation state `func_800B3F84` is handed: the
/// `GpAnimCtx`, the twenty `AnimationSlot`s the tick walks and the pose buffer.
/// The two `MATRIX`es are the model's light and colour matrices, published
/// through `TmdObject::lightMtx` / `colorMtx`. The three code/phase pairs at
/// the end are requests the setters arm and the tick consumes.
typedef struct Actor120500Work {
    /* 0x000 */ ActorAnimRig20 rig;
    /* 0x474 */ MATRIX         field_474; // light matrix, into TmdObject::lightMtx
    /* 0x494 */ MATRIX         field_494; // colour matrix, into TmdObject::colorMtx
    /* 0x4B4 */ Task*          field_4B4; // task in pointer slot 3, the animation messages' target
    /* 0x4B8 */ s16            field_4B8;
    /* 0x4BA */ s16            field_4BA;
    /* 0x4BC */ byte           pad_4BC[0x4];
    /* 0x4C0 */ u16            field_4C0;
    /* 0x4C2 */ s16            field_4C2;
    /* 0x4C4 */ byte           pad_4C4[0x4];
    /* 0x4C8 */ u16            field_4C8;
    /* 0x4CA */ s16            field_4CA;
} Actor120500Work;
STATIC_ASSERT_SIZEOF(Actor120500Work, 0x4CC);

/// Scratch buffer `func_actor_120500_8013241C` fills twice in one tick: as the
/// `AnimationPlayRequest` it hands the task in pointer slot 3 with message 0x3E8, then as
/// the model's part-1 translation `func_800D7A9C` draws with. The two uses
/// cannot overlap, and the frame keeps them in one 0x14-byte stack slot.
typedef union Actor120500Args {
    /* 0x0 */ AnimationPlayRequest msg; // message 0x3E8 payload
    /* 0x0 */ VECTOR               pos; // model part-1 translation
} Actor120500Args;

/// The actor task, published by `func_actor_120500_801322A0` so the setters,
/// which take no task, can reach its work block.
extern Task* D_actor_120500_80138454;

/// The actor's five-entry task table: 0 the streamed sequence
/// (`func_actor_120500_80131E58`), 1 the fade from black, 2 the fade to black,
/// 3 `taskKill`, 4 the actor itself.
extern TaskDesc D_actor_120500_80138418[];

/// Animation banks `func_800B3F84` seeds the work block from.
extern AnimationSet* D_actor_120500_80138088[2];

/// Message table the actor answers with: 0x7D5 shows or hides the model, 0x7D4
/// places it.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s32, GpXformArg*);
        void (*call1)(Task*, s32, s32);
    } handler;
} Actor120500MessageEntry;
STATIC_ASSERT_SIZEOF(Actor120500MessageEntry, 8);

extern Actor120500MessageEntry D_actor_120500_80138408[2];

/// Equipped-weapon id and the flag that selects which block of animation sets
/// it indexes (`+1` when set to 1, `+0x22` otherwise).

/// Flags the tick checks before bringing the actor up (`Gp_StateC08.field_A` /
/// `gDisplayState.pendingMode`), and the one it raises alongside the view tasks
/// (`gDisplayState.control.flags.flipMode`).

/// Animation-set table handed to the task in pointer slot 3 as message 0x3F4's
/// `AnimationPlayRequest::source`; the messages select sets 0, 1 and 2 of it.
extern AnimationSet* D_actor_120500_8013807C[];

/// Placement records sent to that same task as message 0x3E9, passed by
/// address.
extern GpXformArg D_actor_120500_80138090;
extern GpXformArg D_actor_120500_801380A8;

/// Pair of blocks `func_actor_120500_8013241C` passes to `func_800E8634`.
extern GpEvsCmd D_actor_120500_801380D8[];
extern GpEvsCmd D_actor_120500_80138318[];

/// The actor's own placement, sent to itself as message 0x7D4.
extern GpXformArg D_actor_120500_801380C0;

void func_actor_120500_801328C0(s16);
void func_actor_120500_801328E0(s16);
void func_actor_120500_80132900(s16);
void func_actor_120500_80132920(void);

extern TmdSource D_actor_120500_8013762C;
void             func_actor_120500_80131E58(Task*);
void             func_actor_120500_8013241C(Task*);
void             func_actor_120500_80132708(Task*);
void             func_actor_120500_801327E4(Task*);
void             func_actor_120500_80132A04(Task*, s32, s32);
void             func_actor_120500_80132A74(Task*, s32, GpXformArg*);

TmdBone D_actor_120500_80132B0C[20] = {
#include "assets/actor_120500_model_0580C_skeleton.inc"
};

u32 D_actor_120500_80132DDC[20] = {
#include "assets/actor_120500_model_0580C_partVerts.inc"
};

SVECTOR D_actor_120500_80132E2C[300] = {
#include "assets/actor_120500_model_0580C_verts.inc"
};

SVECTOR D_actor_120500_8013378C[298] = {
#include "assets/actor_120500_model_0580C_normals.inc"
};

u32 D_actor_120500_801340DC[3412] = {
#include "assets/actor_120500_model_0580C_stream.inc"
};

TmdSource D_actor_120500_8013762C = {
    0,
    18224,
    5696,
    20,
    D_actor_120500_80132DDC,
    D_actor_120500_80132E2C,
    D_actor_120500_8013378C,
    D_actor_120500_80132B0C,
    D_actor_120500_801340DC,
};

AnimationPackedPose D_actor_120500_80137650[2] = {
#include "assets/actor_120500_animation_05BC8_bank1.inc"
};

AnimationPackedRotation D_actor_120500_80137668[47] = {
#include "assets/actor_120500_animation_05BC8_bank4.inc"
};

AnimationRecord D_actor_120500_80137724[167] = {
#include "assets/actor_120500_animation_05BC8_records.inc"
};

u16 D_actor_120500_801379C0[20] = {
#include "assets/actor_120500_animation_05BC8_indices.inc"
};

AnimationSet D_actor_120500_801379E8 = {
    D_actor_120500_80137724,
    D_actor_120500_801379C0,
    { NULL, D_actor_120500_80137650, NULL, NULL, D_actor_120500_80137668, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120500_80137A10[6] = {
#include "assets/actor_120500_animation_05ECC_bank1.inc"
};

AnimationPackedRotation D_actor_120500_80137A58[46] = {
#include "assets/actor_120500_animation_05ECC_bank4.inc"
};

AnimationRecord D_actor_120500_80137B10[109] = {
#include "assets/actor_120500_animation_05ECC_records.inc"
};

u16 D_actor_120500_80137CC4[20] = {
#include "assets/actor_120500_animation_05ECC_indices.inc"
};

AnimationSet D_actor_120500_80137CEC = {
    D_actor_120500_80137B10,
    D_actor_120500_80137CC4,
    { NULL, D_actor_120500_80137A10, NULL, NULL, D_actor_120500_80137A58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120500_80137D14[2] = {
#include "assets/actor_120500_animation_0607C_bank1.inc"
};

AnimationPackedRotation D_actor_120500_80137D2C[6] = {
#include "assets/actor_120500_animation_0607C_bank4.inc"
};

AnimationRecord D_actor_120500_80137D44[76] = {
#include "assets/actor_120500_animation_0607C_records.inc"
};

u16 D_actor_120500_80137E74[20] = {
#include "assets/actor_120500_animation_0607C_indices.inc"
};

AnimationSet D_actor_120500_80137E9C = {
    D_actor_120500_80137D44,
    D_actor_120500_80137E74,
    { NULL, D_actor_120500_80137D14, NULL, NULL, D_actor_120500_80137D2C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120500_80137EC4[2] = {
#include "assets/actor_120500_animation_06234_bank1.inc"
};

AnimationPackedRotation D_actor_120500_80137EDC[8] = {
#include "assets/actor_120500_animation_06234_bank4.inc"
};

AnimationRecord D_actor_120500_80137EFC[76] = {
#include "assets/actor_120500_animation_06234_records.inc"
};

u16 D_actor_120500_8013802C[20] = {
#include "assets/actor_120500_animation_06234_indices.inc"
};

AnimationSet D_actor_120500_80138054 = {
    D_actor_120500_80137EFC,
    D_actor_120500_8013802C,
    { NULL, D_actor_120500_80137EC4, NULL, NULL, D_actor_120500_80137EDC, NULL, NULL, NULL },
};

AnimationSet* D_actor_120500_8013807C[3] = {
    &D_actor_120500_80137CEC,
    &D_actor_120500_80137E9C,
    &D_actor_120500_80138054,
};

AnimationSet* D_actor_120500_80138088[2] = {
    NULL,
    &D_actor_120500_801379E8,
};

GpXformArg D_actor_120500_80138090 = { { 6330, -3200, -4900, 0 }, { 0, 3584, 0, 0 } };

GpXformArg D_actor_120500_801380A8 = { { 787, 0, 7000, 0 }, { 0, 3584, 0, 0 } };

GpXformArg D_actor_120500_801380C0 = { { 800, -0x2EE0, -2750, 0 }, { 0, 2048, 0, 0 } };

GpEvsCmd D_actor_120500_801380D8[24] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_120500_801328C0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_120500_80132900 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_120500_80132900 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_120500_801328E0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_120500_80132900 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_120500_801328C0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_120500_801328C0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_120500_801328C0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_actor_120500_801328C0 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_120500_80138318[10] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_120500_80132920 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

Actor120500MessageEntry D_actor_120500_80138408[2] = {
    { 2005, { .call1 = func_actor_120500_80132A04 } },
    { 2004, { .call0 = func_actor_120500_80132A74 } },
};

TaskDesc D_actor_120500_80138418[3] = {
    { 0, 192, func_actor_120500_80131E58, { .model = NULL } },
    { 0, 192, func_actor_120500_80132708, { .model = NULL } },
    { 0, 192, func_actor_120500_801327E4, { .model = NULL } },
};

TaskDesc D_actor_120500_8013843C = { 0, 192, taskKill, { .model = NULL } };

TaskDesc D_actor_120500_80138448 = { 257, 192, func_actor_120500_8013241C, { .model = &D_actor_120500_8013762C } };

Task* D_actor_120500_80138454 = NULL;

static void func_actor_120500_80132028(Task* arg0);
static void func_actor_120500_801322A0(Task* task);

/// Entry 0 of the task table: plays a streamed sequence, then restores the
/// scene. It looks up the stream slot for the current location with view 0x64
/// and enqueues CD command 0x61 on it, turns the display on with sound cue
/// 0x521E0007 once the queue reports ready, and waits for the stream to end or
/// for the pad to cut it short. After the restore it spawns the fade from
/// black, clears the image buffers, and kills itself.
void func_actor_120500_80131E58(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    s16         slot;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            goto advance;
        case 1:
            key          = gGameSession->at4;
            key.loc.view = 0x64;
            slot         = Stream_FindSlot((u8*)&key, 0, 0);
            slotParam[0] = slot;
            CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            goto advance;
        case 2:
            if (queue->field_1FA == 0) {
                return;
            }
            SndEvt_EnqueueType6(0x521E0007, 0, 0);
            SetDispMask(1);
            goto advance;
        case 3:
            if (CdCmd_IsIdle() & 0xFFFF) {
                SetDispMask(0);
                goto advance;
            }
            if (Pad_CheckFlag800() == 0) {
                return;
            }
            SetDispMask(0);
            CdCmd_ActivatePhase1();
            goto advance;
        case 4:
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
        advance:
            task->state = task->state + 1;
            return;
        case 5:
            if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
                return;
            }
            Task_SpawnOnDefaultList(D_actor_120500_80138418, 1, 8, 0);
            Mem_Set(Fs_ImgBuffers, 0, 0x25800);
            SetDispMask(1);
            taskKill(task);
            Display_ResetHeapWrapper();
            return;
    }
}

/// Per-frame request handler for the pair at `field_4B8` / `field_4BA`, stepped
/// by the tick body and armed by `func_actor_120500_801328C0`. Every tick first
/// sends message 0x3ED to the task at `field_4B4`, then dispatches on the code:
/// 2 raises the override vector and installs animation set 0 once before
/// sending placement record `D_actor_120500_80138090` on every tick -- the only
/// code that does not clear itself; 3 spawns the fade from black, drops the
/// override and installs set 1 with `D_actor_120500_801380A8`; 4 installs set 2
/// with the 8-frame blend; 5 sends the actor its own message 0x7D5 with payload
/// 2, hiding the model; and 6 sends message 0x3E8 with the equipped-weapon
/// animation, picked the same way `func_actor_120500_8013241C` picks it. Every
/// other code, 0 and 1 included, just clears the request.
static void func_actor_120500_80132028(Task* arg0)
{
    Actor120500Work*      work;
    Actor120500Work*      w;
    Actor120500Work*      w2;
    Actor120500Work*      w3;
    SVECTOR               vec;
    AnimationPlayRequest  msg;
    AnimationPlayRequest* p;
    s32                   anim;
    s32                   base;

    work = (Actor120500Work*)arg0->work;
    if (work->field_4B4 != NULL) {
        Gp_DispatchMsg(work->field_4B4, 0x3ED, 0, 0);
    }
    switch ((u16)work->field_4B8) {
        case 0:
        case 1:
            break;
        case 2:
            switch ((u16)work->field_4BA) {
                case 0:
                    vec.vx = 0x960;
                    vec.vy = 0x960;
                    vec.vz = 0x960;
                    Gp_SetOverrideVec(&vec);
                    w3 = (Actor120500Work*)arg0->work;
                    p  = &msg;
                    if (w3->field_4B4 != NULL) {
                        msg.source.sets         = D_actor_120500_8013807C;
                        msg.animationId         = 0;
                        msg.blend               = ANIMATION_BLEND_RESET;
                        msg.blendFrames         = 0;
                        p->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                        Gp_DispatchMsgPtr(w3->field_4B4, ANIMATION_MESSAGE_INSTALL_AND_PLAY, p, 0);
                    }
                    work->field_4BA = work->field_4BA + 1;
                    /* fallthrough */
                case 1:
                    Gp_DispatchMsgPtr(((Actor120500Work*)arg0->work)->field_4B4, 0x3E9,
                                      &D_actor_120500_80138090, 0);
                    return;
            }
            return;
        case 3:
            Task_SpawnFromTable(D_actor_120500_80138418, 1, 8, 0);
            w = (Actor120500Work*)arg0->work;
            Gp_SetOverrideVec(NULL);
            Gp_DispatchMsg(w->field_4B4, 0x3F3, 1, 0);
            Gp_DispatchMsgPtr(w->field_4B4, 0x3E9, &D_actor_120500_801380A8, 0);
            w2 = (Actor120500Work*)arg0->work;
            p  = &msg;
            if (w2->field_4B4 != NULL) {
                msg.source.sets         = D_actor_120500_8013807C;
                p->animationId          = 1;
                msg.blend               = ANIMATION_BLEND_RESET;
                msg.blendFrames         = 0;
                p->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                Gp_DispatchMsgPtr(w2->field_4B4, ANIMATION_MESSAGE_INSTALL_AND_PLAY, p, 0);
            }
            break;
        case 4:
            w2 = (Actor120500Work*)arg0->work;
            p  = &msg;
            if (w2->field_4B4 != NULL) {
                msg.source.sets         = D_actor_120500_8013807C;
                p->animationId          = 2;
                p->blend                = ANIMATION_BLEND_INTERPOLATE;
                p->blendFrames          = 8;
                p->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                Gp_DispatchMsgPtr(w2->field_4B4, ANIMATION_MESSAGE_INSTALL_AND_PLAY, p, 0);
            }
            break;
        case 5:
            Gp_DispatchMsg(arg0, 0x7D5, 2, 0);
            break;
        case 6:
            base = Player_Status.weapon;
            if (Mc_SaveData[0].state.characterId == 1) {
                anim = base + 1;
            } else {
                anim = base + 0x22;
            }
            msg.source.index         = anim;
            msg.animationId          = 1;
            msg.blend                = ANIMATION_BLEND_RESET;
            msg.blendFrames          = 0;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            Gp_DispatchMsgPtr(work->field_4B4, ANIMATION_MESSAGE_PLAY, &msg, 0);
            break;
    }
    work->field_4B8 = 0;
}

/// Initialize the cutscene actor's model and animations.
///
/// Uses the area placement for resource-entry 0x65, or the end record when
/// that entry is absent. Allocation failure kills `task`.
static void func_actor_120500_801322A0(Task* task)
{
    enum { TEXTURE_RESOURCE_ENTRY_ID = 0x65 };

    Actor120500Work* work;
    Actor120500Work* allocatedWork;
    Actor120500Work* slotsWork;
    TmdObject*       tmd;
    GfxCoord*        coord;
    AreaPlacement*   place;
    s32              slotIndex;
    u8               entryId;

    tmd           = task->extra.tmd;
    coord         = tmd->coords;
    allocatedWork = Mem_Malloc(sizeof(Actor120500Work), 0);
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(task);
        return;
    }
    work = allocatedWork;
    Mem_Set(work, 0, sizeof(*work));
    work->field_4B4         = gameGetPtrSlot(3);
    D_actor_120500_80138454 = task;
    coord->parent           = &gGfxViewCoord;
    tmd->lightMtx           = &work->field_474;
    tmd->flags              = 0;
    tmd->colorMtx           = &work->field_494;
    place                   = Gp_GetNestedAreaRec(&gGameSession->at4.loc)->field_0;
    entryId                 = place->entryId;
    while (entryId != AREA_PLACEMENT_END) {
        if (entryId == TEXTURE_RESOURCE_ENTRY_ID) {
            break;
        }
        place++;
        entryId = place->entryId;
    }
    Gp_SetTmdBytes(tmd, place->texturePageOffset, place->clutRowOffset);
    func_800B3F84(&work->rig.anim, D_actor_120500_80138088, tmd, work->rig.poses, work->rig.slots);
    slotsWork      = (Actor120500Work*)task->work;
    task->msgTable = D_actor_120500_80138408;
    slotIndex      = 1;
    do {
        slotsWork->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        Gp_AnimResetSlot(&slotsWork->rig.anim, (u16)slotIndex, 1);
        slotIndex++;
    } while ((u16)slotIndex < ARRAY_SIZE(slotsWork->rig.slots));
}

/// Per-frame body of the actor task, entry 4 of the task table. State 0 waits
/// until `Gp_StateC08.field_A` is not 1 and `gDisplayState.pendingMode` is clear, then brings the actor
/// up through `func_actor_120500_801322A0`, sends the task in pointer slot 3
/// the equipped-weapon animation as message 0x3E8 and installs the two
/// `func_800E8634` blocks; state 1 kills the actor once the session's
/// `eventState` clears.
///
/// Every state then steps the request handler, ticks the nineteen animation
/// slots past slot 0 and walks the slots to the first whose `flags` bit 0 is
/// clear. Request code 1 at 0x4C0 allocates the model's buffers, spawns the
/// fade from black and places the actor with its own placement record. At
/// 0x4C8, code 1 spawns the fade to black and code 2 sends message 0x3F3,
/// spawns the streamed sequence, raises `gDisplayState.control.flags.flipMode` and spawns the view
/// tasks. The model's part-1 translation goes to `func_800D7A9C` last.
///
/// The request 0x4C8 dispatch is written with gotos: the labels reproduce
/// retail's block layout, where the three clear sites sit at the end of their
/// own arms.
void func_actor_120500_8013241C(Task* arg0)
{
    Actor120500Work* work;
    Actor120500Work* slotsWork;
    Actor120500Work* w;
    Actor120500Args  args;
    TmdObject*       mdl;
    s32              anim;
    s32              code;
    s32              i;

    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.field_A != 1 && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                func_actor_120500_801322A0(arg0);
                anim = Player_Status.weapon;
                if (Mc_SaveData[0].state.characterId == 1) {
                    anim = anim + 1;
                } else {
                    anim = anim + 0x22;
                }
                args.msg.source.index         = anim;
                args.msg.animationId          = 1;
                args.msg.blend                = ANIMATION_BLEND_INTERPOLATE;
                args.msg.blendFrames          = 10;
                args.msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, &args.msg, 0);
                func_800E3FAC(0xA2, 0xD);
                func_800E8634(D_actor_120500_801380D8, 0, D_actor_120500_80138318);
                arg0->state += 1;
                break;
            }
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(arg0, 0);
                return;
            }
            break;
    }

    func_actor_120500_80132028(arg0);
    work      = (Actor120500Work*)arg0->work;
    slotsWork = work;

    i = 1;
    do {
        Gp_AnimTickIndex(&slotsWork->rig.anim, (u16)i);
        i++;
    } while ((u16)i < 0x14U);

    i = 1;
loop_slots:
    if ((slotsWork->rig.slots[(u16)i].flags & ANIMATION_SLOT_REACHED_END) != 0) {
        i++;
        if ((u16)i < 0x14U) {
            goto loop_slots;
        }
    }

    if (work->field_4C0 != 0) {
        if (work->field_4C0 == 1) {
            Tmd_AllocBuffers(arg0->extra.tmd);
            Task_SpawnFromTable(D_actor_120500_80138418, 1, 8, 0);
            Gp_DispatchMsgPtr(arg0, 0x7D4, &D_actor_120500_801380C0, 0);
        }
    }
    work->field_4C0 = 0;

    w    = (Actor120500Work*)arg0->work;
    code = w->field_4C8;
    if (code != 1) {
        if (code >= 2) {
            if (code != 2) {
                w->field_4C8 = 0;
                goto done_4C8;
            } else {
                goto do_4C8_case2;
            }
        } else {
            goto clear_4C8;
        }
    } else {
        goto do_4C8_case1;
    }
do_4C8_case1:
    Task_SpawnFromTable(D_actor_120500_80138418, 2, 8, 0);
    w->field_4C8 = 0;
    goto done_4C8;
do_4C8_case2:
    Gp_DispatchMsg(w->field_4B4, 0x3F3, 2, 0);
    Display_SpawnWithOt(D_actor_120500_80138418, 0, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
clear_4C8:
    w->field_4C8 = 0;
done_4C8:

    mdl         = arg0->extra.tmd;
    args.pos.vx = arg0->extra.tmd->coords[1].workm.t[0];
    args.pos.vy = arg0->extra.tmd->coords[1].workm.t[1];
    args.pos.vz = arg0->extra.tmd->coords[1].workm.t[2];
    func_800D7A9C(mdl, &args.pos, 0, 3);
}

/// Fade from black, entry 1 of the actor's task table.
///
/// State 0 allocates the channel block and seeds all three channels at 0xFF;
/// a failed allocation kills the task. State 1 runs every frame: it draws a
/// subtractive `Fade_DrawOverlay` tinted `r`/`g`/`r` (`b` is stepped but never
/// drawn), then lowers all three channels by `Task::spawnArg1`, the fade rate.
/// Once `r` has gone negative the screen is clear and the task kills itself.
void func_actor_120500_80132708(Task* arg0)
{
    OverlayFadeWork* fade;
    OverlayFadeWork* alloc;

    fade = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = alloc;
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
            fade->r = (s16)((u16)fade->r - (u16)arg0->spawnArg1.value);
            fade->g = (s16)((u16)fade->g - (u16)arg0->spawnArg1.value);
            fade->b = (s16)((u16)fade->b - (u16)arg0->spawnArg1.value);
            if (fade->r >= 0) {
                return;
            }
            taskKill(arg0);
            break;
    }
}

/// Fade to black, entry 2 of the actor's task table.
///
/// State 0 allocates the channel block and clears it; a failed allocation
/// kills the task. State 1 runs every frame: it draws a subtractive
/// `Fade_DrawOverlay` tinted `r`/`g`/`r` (`b` is stepped but never drawn),
/// then raises all three channels by `Task::spawnArg1`, the fade rate. Once
/// `r` reaches 0x100 the screen is black and the task kills itself.
void func_actor_120500_801327E4(Task* arg0)
{
    OverlayFadeWork* work;
    OverlayFadeWork* alloc;

    work = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0;
            work->g      = 0;
            work->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r += (u16)arg0->spawnArg1.value;
            work->g += (u16)arg0->spawnArg1.value;
            work->b += (u16)arg0->spawnArg1.value;
            if (work->r >= 0x100) {
                taskKill(arg0);
            }
            break;
    }
}

/// Request setters, reached from the tables in the actor's data: each arms one
/// code/phase pair of the actor's work block with `arg0` and restarts its
/// phase. This one arms the pair `func_actor_120500_80132028` consumes; the
/// next two arm the pairs the tick consumes at 0x4C0 and 0x4C8.
void func_actor_120500_801328C0(s16 arg0)
{
    Actor120500Work* work = D_actor_120500_80138454->work;

    work->field_4B8 = arg0;
    work->field_4BA = 0;
}

void func_actor_120500_801328E0(s16 arg0)
{
    Actor120500Work* work = D_actor_120500_80138454->work;

    work->field_4C0 = arg0;
    work->field_4C2 = 0;
}

void func_actor_120500_80132900(s16 arg0)
{
    Actor120500Work* work = D_actor_120500_80138454->work;

    work->field_4C8 = arg0;
    work->field_4CA = 0;
}

/// Puts the actor back to rest: plays sound cue `0x521E0007`, sends the actor
/// its own message 0x7D5 with payload 2, hiding the model, and clears the
/// three request codes the setters above arm. The task at `field_4B4` then
/// gets animation set 2 of `D_actor_120500_8013807C` (message 0x3F4), message
/// 0x3F3 with payload 1, and the placement record `D_actor_120500_801380A8`
/// as message 0x3E9, with the override vector cleared in between.
void func_actor_120500_80132920(void)
{
    Task*                actor;
    Actor120500Work*     work;
    Actor120500Work*     animWork;
    AnimationPlayRequest msg;

    actor = D_actor_120500_80138454;
    work  = actor->work;
    SndEvt_EnqueueType7(0x521E0007, 0xA);
    Gp_DispatchMsg(actor, 0x7D5, 2, 0);
    work->field_4B8 = 0;
    work->field_4C0 = 0;
    work->field_4C8 = 0;
    animWork        = actor->work;
    if (animWork->field_4B4 != NULL) {
        msg.source.sets          = D_actor_120500_8013807C;
        msg.animationId          = 2;
        msg.blend                = ANIMATION_BLEND_RESET;
        msg.blendFrames          = 0;
        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        Gp_DispatchMsgPtr(animWork->field_4B4, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
    }
    work = actor->work;
    Gp_SetOverrideVec(NULL);
    Gp_DispatchMsg(work->field_4B4, 0x3F3, 1, 0);
    Gp_DispatchMsgPtr(work->field_4B4, 0x3E9, &D_actor_120500_801380A8, 0);
}

/// Message 0x7D5 handler: shows or hides the task's model. Payload 0 hides it
/// (sets `TmdObject` flag 0x80), 1 shows it and clears flag 0x4, and 2 hides
/// it and sets 0x4, which keeps `Tmd_AllocMissingBuffers` from giving it
/// buffers again. Payload 2 sets 0x4 and falls into payload 0, rather than
/// setting both bits at once, and the branch layout follows that. `arg1` is
/// the message id.
void func_actor_120500_80132A04(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = task->extra.tmd;
    switch (arg2) {
        case 2:
            extra->flags = extra->flags | TMD_OBJECT_SKIP_AUTO_BUFFER;
            /* fallthrough */
        case 0:
            extra->flags = extra->flags | TMD_OBJECT_HIDDEN;
            return;
        case 1:
            extra->flags = extra->flags & (u16) ~(TMD_OBJECT_HIDDEN | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
    }
}

/// Message 0x7D4 handler: places the task's model in the world. The model's
/// coordinate is parented to the view coordinate, takes `placement`'s
/// position as its translation and its rotation applied Y, then X, then Z.
/// `arg1` is the message id.
void func_actor_120500_80132A74(Task* task, s32 arg1, GpXformArg* placement)
{
    GfxCoord* coord;
    MATRIX*   mtx;

    coord             = task->extra.tmd->coords;
    coord->parent     = &gGfxViewCoord;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
