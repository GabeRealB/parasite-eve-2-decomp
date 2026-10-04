#include "kyle/kyle_800102.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "kyle_800102_private.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "weapons/weapon.h"
#include "../../shared/grenade_shell.h"

#include "../../shared/grenade_shell_spawn.inc.c"

#include "../../shared/grenade_shell_fly.inc.c"

#include "../../shared/grenade_shell_blast.inc.c"

#include "../../shared/grenade_shell_exit.inc.c"
void func_kyle_800102_801682B4(Task* task)
{
    TaskFunc states[4] = {
        grenadeShellSpawn,
        grenadeShellFly,
        grenadeShellBlast,
        grenadeShellExit,
    };

    states[task->state](task);
}

static TmdBone _gKyle800102KyleMadiganBodySkeleton[20] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

static u32 _gKyle800102KyleMadiganBodyPartVerts[20] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

static SVECTOR _gKyle800102KyleMadiganBodyVerts[300] = {
#include "assets/kyle_madigan_body_verts.inc"
};

static SVECTOR _gKyle800102KyleMadiganBodyNormals[298] = {
#include "assets/kyle_madigan_body_normals.inc"
};

static u32 _gKyle800102KyleMadiganBodyStream[3412] = {
#include "assets/kyle_madigan_body_stream.inc"
};

TmdSource D_kyle_800102_8016CE38 = {
    0,
    18224,
    5696,
    20,
    _gKyle800102KyleMadiganBodyPartVerts,
    _gKyle800102KyleMadiganBodyVerts,
    _gKyle800102KyleMadiganBodyNormals,
    _gKyle800102KyleMadiganBodySkeleton,
    _gKyle800102KyleMadiganBodyStream,
};

static TmdBone _gKyle800102KyleMadiganHandRightSkeleton[1] = {
#include "assets/kyle_madigan_hand_right_skeleton.inc"
};

static u32 _gKyle800102KyleMadiganHandRightPartVerts[1] = {
#include "assets/kyle_madigan_hand_right_partVerts.inc"
};

static SVECTOR _gKyle800102KyleMadiganHandRightVerts[23] = {
#include "assets/kyle_madigan_hand_right_verts.inc"
};

static SVECTOR _gKyle800102KyleMadiganHandRightNormals[23] = {
#include "assets/kyle_madigan_hand_right_normals.inc"
};

static u32 _gKyle800102KyleMadiganHandRightStream[166] = {
#include "assets/kyle_madigan_hand_right_stream.inc"
};

TmdSource D_kyle_800102_8016D28C = {
    0,
    1148,
    0,
    1,
    _gKyle800102KyleMadiganHandRightPartVerts,
    _gKyle800102KyleMadiganHandRightVerts,
    _gKyle800102KyleMadiganHandRightNormals,
    _gKyle800102KyleMadiganHandRightSkeleton,
    _gKyle800102KyleMadiganHandRightStream,
};

static TmdBone _gKyle800102Kyle800101Model05174Skeleton[1] = {
#include "assets/kyle_800101_model_05174_skeleton.inc"
};

static u32 _gKyle800102Kyle800101Model05174PartVerts[1] = {
#include "assets/kyle_800101_model_05174_partVerts.inc"
};

static SVECTOR _gKyle800102Kyle800101Model05174Verts[27] = {
#include "assets/kyle_800101_model_05174_verts.inc"
};

static SVECTOR _gKyle800102Kyle800101Model05174Normals[27] = {
#include "assets/kyle_800101_model_05174_normals.inc"
};

static u32 _gKyle800102Kyle800101Model05174Stream[189] = {
#include "assets/kyle_800101_model_05174_stream.inc"
};

TmdSource D_kyle_800102_8016D77C = {
    0,
    1328,
    0,
    1,
    _gKyle800102Kyle800101Model05174PartVerts,
    _gKyle800102Kyle800101Model05174Verts,
    _gKyle800102Kyle800101Model05174Normals,
    _gKyle800102Kyle800101Model05174Skeleton,
    _gKyle800102Kyle800101Model05174Stream,
};

static TmdBone _gKyle800102KyleMadiganLeftSkeleton[1] = {
#include "assets/kyle_madigan_left_skeleton.inc"
};

static u32 _gKyle800102KyleMadiganLeftPartVerts[1] = {
#include "assets/kyle_madigan_left_partVerts.inc"
};

static SVECTOR _gKyle800102KyleMadiganLeftVerts[23] = {
#include "assets/kyle_madigan_left_verts.inc"
};

static SVECTOR _gKyle800102KyleMadiganLeftNormals[23] = {
#include "assets/kyle_madigan_left_normals.inc"
};

static u32 _gKyle800102KyleMadiganLeftStream[166] = {
#include "assets/kyle_madigan_left_stream.inc"
};

TmdSource D_kyle_800102_8016DBD0 = {
    0,
    1148,
    0,
    1,
    _gKyle800102KyleMadiganLeftPartVerts,
    _gKyle800102KyleMadiganLeftVerts,
    _gKyle800102KyleMadiganLeftNormals,
    _gKyle800102KyleMadiganLeftSkeleton,
    _gKyle800102KyleMadiganLeftStream,
};

static TmdBone _gKyle800102KyleMadiganHandLeftSkeleton[1] = {
#include "assets/kyle_madigan_hand_left_skeleton.inc"
};

static u32 _gKyle800102KyleMadiganHandLeftPartVerts[1] = {
#include "assets/kyle_madigan_hand_left_partVerts.inc"
};

static SVECTOR _gKyle800102KyleMadiganHandLeftVerts[27] = {
#include "assets/kyle_madigan_hand_left_verts.inc"
};

static SVECTOR _gKyle800102KyleMadiganHandLeftNormals[27] = {
#include "assets/kyle_madigan_hand_left_normals.inc"
};

static u32 _gKyle800102KyleMadiganHandLeftStream[189] = {
#include "assets/kyle_madigan_hand_left_stream.inc"
};

TmdSource D_kyle_800102_8016E0C0 = {
    0,
    1328,
    0,
    1,
    _gKyle800102KyleMadiganHandLeftPartVerts,
    _gKyle800102KyleMadiganHandLeftVerts,
    _gKyle800102KyleMadiganHandLeftNormals,
    _gKyle800102KyleMadiganHandLeftSkeleton,
    _gKyle800102KyleMadiganHandLeftStream,
};

static TmdBone _gKyle800102Model069CCSkeleton[1] = {
#include "assets/kyle_800102_model_069CC_skeleton.inc"
};

static u32 _gKyle800102Model069CCPartVerts[1] = {
#include "assets/kyle_800102_model_069CC_partVerts.inc"
};

static SVECTOR _gKyle800102Model069CCVerts[52] = {
#include "assets/kyle_800102_model_069CC_verts.inc"
};

static SVECTOR _gKyle800102Model069CCNormals[50] = {
#include "assets/kyle_800102_model_069CC_normals.inc"
};

static u32 _gKyle800102Model069CCStream[320] = {
#include "assets/kyle_800102_model_069CC_stream.inc"
};

TmdSource D_kyle_800102_8016E93C = {
    0,
    2292,
    0,
    1,
    _gKyle800102Model069CCPartVerts,
    _gKyle800102Model069CCVerts,
    _gKyle800102Model069CCNormals,
    _gKyle800102Model069CCSkeleton,
    _gKyle800102Model069CCStream,
};
