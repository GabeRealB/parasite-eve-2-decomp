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

static void _grenadeShellExit(Task* task);

#include "../../shared/grenade_shell_spawn.inc.c"

#include "../../shared/grenade_shell_fly.inc.c"

#include "../../shared/grenade_shell_blast.inc.c"

#include "../../shared/grenade_shell_exit.inc.c"
#include "gameplay/animation.h"
void func_kyle_800102_801682B4(Task* task)
{
    TaskFunc states[4] = {
        grenadeShellSpawn,
        grenadeShellFly,
        _grenadeShellBlast,
        _grenadeShellExit,
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

static AnimationPackedPose _gKyle800102Animation070F8Bank1[2] = {
#include "assets/kyle_800102_animation_070F8_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation070F8Bank4[24] = {
#include "assets/kyle_800102_animation_070F8_bank4.inc"
};

static AnimationRecord _gKyle800102Animation070F8Records[90] = {
#include "assets/kyle_800102_animation_070F8_records.inc"
};

static u16 _gKyle800102Animation070F8Indices[20] = {
#include "assets/kyle_800102_animation_070F8_indices.inc"
};

static AnimationSet _gKyle800102Animation070F8 = {
    _gKyle800102Animation070F8Records,
    _gKyle800102Animation070F8Indices,
    { NULL, _gKyle800102Animation070F8Bank1, NULL, NULL, _gKyle800102Animation070F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation07978Bank1[11] = {
#include "assets/kyle_800102_animation_07978_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation07978Bank4[201] = {
#include "assets/kyle_800102_animation_07978_bank4.inc"
};

static AnimationRecord _gKyle800102Animation07978Records[290] = {
#include "assets/kyle_800102_animation_07978_records.inc"
};

static u16 _gKyle800102Animation07978Indices[20] = {
#include "assets/kyle_800102_animation_07978_indices.inc"
};

static AnimationSet _gKyle800102Animation07978 = {
    _gKyle800102Animation07978Records,
    _gKyle800102Animation07978Indices,
    { NULL, _gKyle800102Animation07978Bank1, NULL, NULL, _gKyle800102Animation07978Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation081E4Bank1[15] = {
#include "assets/kyle_800102_animation_081E4_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation081E4Bank4[171] = {
#include "assets/kyle_800102_animation_081E4_bank4.inc"
};

static AnimationRecord _gKyle800102Animation081E4Records[303] = {
#include "assets/kyle_800102_animation_081E4_records.inc"
};

static u16 _gKyle800102Animation081E4Indices[20] = {
#include "assets/kyle_800102_animation_081E4_indices.inc"
};

static AnimationSet _gKyle800102Animation081E4 = {
    _gKyle800102Animation081E4Records,
    _gKyle800102Animation081E4Indices,
    { NULL, _gKyle800102Animation081E4Bank1, NULL, NULL, _gKyle800102Animation081E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation08AB8Bank1[16] = {
#include "assets/kyle_800102_animation_08AB8_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation08AB8Bank4[182] = {
#include "assets/kyle_800102_animation_08AB8_bank4.inc"
};

static AnimationRecord _gKyle800102Animation08AB8Records[315] = {
#include "assets/kyle_800102_animation_08AB8_records.inc"
};

static u16 _gKyle800102Animation08AB8Indices[20] = {
#include "assets/kyle_800102_animation_08AB8_indices.inc"
};

static AnimationSet _gKyle800102Animation08AB8 = {
    _gKyle800102Animation08AB8Records,
    _gKyle800102Animation08AB8Indices,
    { NULL, _gKyle800102Animation08AB8Bank1, NULL, NULL, _gKyle800102Animation08AB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation08F78Bank1[7] = {
#include "assets/kyle_800102_animation_08F78_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation08F78Bank4[105] = {
#include "assets/kyle_800102_animation_08F78_bank4.inc"
};

static AnimationRecord _gKyle800102Animation08F78Records[158] = {
#include "assets/kyle_800102_animation_08F78_records.inc"
};

static u16 _gKyle800102Animation08F78Indices[20] = {
#include "assets/kyle_800102_animation_08F78_indices.inc"
};

static AnimationSet _gKyle800102Animation08F78 = {
    _gKyle800102Animation08F78Records,
    _gKyle800102Animation08F78Indices,
    { NULL, _gKyle800102Animation08F78Bank1, NULL, NULL, _gKyle800102Animation08F78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0951CBank1[10] = {
#include "assets/kyle_800102_animation_0951C_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0951CBank4[131] = {
#include "assets/kyle_800102_animation_0951C_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0951CRecords[180] = {
#include "assets/kyle_800102_animation_0951C_records.inc"
};

static u16 _gKyle800102Animation0951CIndices[20] = {
#include "assets/kyle_800102_animation_0951C_indices.inc"
};

static AnimationSet _gKyle800102Animation0951C = {
    _gKyle800102Animation0951CRecords,
    _gKyle800102Animation0951CIndices,
    { NULL, _gKyle800102Animation0951CBank1, NULL, NULL, _gKyle800102Animation0951CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation09A8CBank1[11] = {
#include "assets/kyle_800102_animation_09A8C_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation09A8CBank4[127] = {
#include "assets/kyle_800102_animation_09A8C_bank4.inc"
};

static AnimationRecord _gKyle800102Animation09A8CRecords[168] = {
#include "assets/kyle_800102_animation_09A8C_records.inc"
};

static u16 _gKyle800102Animation09A8CIndices[20] = {
#include "assets/kyle_800102_animation_09A8C_indices.inc"
};

static AnimationSet _gKyle800102Animation09A8C = {
    _gKyle800102Animation09A8CRecords,
    _gKyle800102Animation09A8CIndices,
    { NULL, _gKyle800102Animation09A8CBank1, NULL, NULL, _gKyle800102Animation09A8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0A298Bank1[10] = {
#include "assets/kyle_800102_animation_0A298_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0A298Bank4[181] = {
#include "assets/kyle_800102_animation_0A298_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0A298Records[284] = {
#include "assets/kyle_800102_animation_0A298_records.inc"
};

static u16 _gKyle800102Animation0A298Indices[20] = {
#include "assets/kyle_800102_animation_0A298_indices.inc"
};

static AnimationSet _gKyle800102Animation0A298 = {
    _gKyle800102Animation0A298Records,
    _gKyle800102Animation0A298Indices,
    { NULL, _gKyle800102Animation0A298Bank1, NULL, NULL, _gKyle800102Animation0A298Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0A768Bank1[9] = {
#include "assets/kyle_800102_animation_0A768_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0A768Bank4[113] = {
#include "assets/kyle_800102_animation_0A768_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0A768Records[148] = {
#include "assets/kyle_800102_animation_0A768_records.inc"
};

static u16 _gKyle800102Animation0A768Indices[20] = {
#include "assets/kyle_800102_animation_0A768_indices.inc"
};

static AnimationSet _gKyle800102Animation0A768 = {
    _gKyle800102Animation0A768Records,
    _gKyle800102Animation0A768Indices,
    { NULL, _gKyle800102Animation0A768Bank1, NULL, NULL, _gKyle800102Animation0A768Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0ACC0Bank1[8] = {
#include "assets/kyle_800102_animation_0ACC0_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0ACC0Bank4[129] = {
#include "assets/kyle_800102_animation_0ACC0_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0ACC0Records[169] = {
#include "assets/kyle_800102_animation_0ACC0_records.inc"
};

static u16 _gKyle800102Animation0ACC0Indices[20] = {
#include "assets/kyle_800102_animation_0ACC0_indices.inc"
};

static AnimationSet _gKyle800102Animation0ACC0 = {
    _gKyle800102Animation0ACC0Records,
    _gKyle800102Animation0ACC0Indices,
    { NULL, _gKyle800102Animation0ACC0Bank1, NULL, NULL, _gKyle800102Animation0ACC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0AEB0Bank1[2] = {
#include "assets/kyle_800102_animation_0AEB0_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0AEB0Bank4[18] = {
#include "assets/kyle_800102_animation_0AEB0_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0AEB0Records[80] = {
#include "assets/kyle_800102_animation_0AEB0_records.inc"
};

static u16 _gKyle800102Animation0AEB0Indices[20] = {
#include "assets/kyle_800102_animation_0AEB0_indices.inc"
};

static AnimationSet _gKyle800102Animation0AEB0 = {
    _gKyle800102Animation0AEB0Records,
    _gKyle800102Animation0AEB0Indices,
    { NULL, _gKyle800102Animation0AEB0Bank1, NULL, NULL, _gKyle800102Animation0AEB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0B620Bank1[13] = {
#include "assets/kyle_800102_animation_0B620_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0B620Bank4[175] = {
#include "assets/kyle_800102_animation_0B620_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0B620Records[242] = {
#include "assets/kyle_800102_animation_0B620_records.inc"
};

static u16 _gKyle800102Animation0B620Indices[20] = {
#include "assets/kyle_800102_animation_0B620_indices.inc"
};

static AnimationSet _gKyle800102Animation0B620 = {
    _gKyle800102Animation0B620Records,
    _gKyle800102Animation0B620Indices,
    { NULL, _gKyle800102Animation0B620Bank1, NULL, NULL, _gKyle800102Animation0B620Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0C030Bank1[16] = {
#include "assets/kyle_800102_animation_0C030_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0C030Bank4[250] = {
#include "assets/kyle_800102_animation_0C030_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0C030Records[326] = {
#include "assets/kyle_800102_animation_0C030_records.inc"
};

static u16 _gKyle800102Animation0C030Indices[20] = {
#include "assets/kyle_800102_animation_0C030_indices.inc"
};

static AnimationSet _gKyle800102Animation0C030 = {
    _gKyle800102Animation0C030Records,
    _gKyle800102Animation0C030Indices,
    { NULL, _gKyle800102Animation0C030Bank1, NULL, NULL, _gKyle800102Animation0C030Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0CCD8Bank1[23] = {
#include "assets/kyle_800102_animation_0CCD8_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0CCD8Bank4[303] = {
#include "assets/kyle_800102_animation_0CCD8_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0CCD8Records[418] = {
#include "assets/kyle_800102_animation_0CCD8_records.inc"
};

static u16 _gKyle800102Animation0CCD8Indices[20] = {
#include "assets/kyle_800102_animation_0CCD8_indices.inc"
};

static AnimationSet _gKyle800102Animation0CCD8 = {
    _gKyle800102Animation0CCD8Records,
    _gKyle800102Animation0CCD8Indices,
    { NULL, _gKyle800102Animation0CCD8Bank1, NULL, NULL, _gKyle800102Animation0CCD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0D084Bank1[5] = {
#include "assets/kyle_800102_animation_0D084_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0D084Bank4[82] = {
#include "assets/kyle_800102_animation_0D084_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0D084Records[118] = {
#include "assets/kyle_800102_animation_0D084_records.inc"
};

static u16 _gKyle800102Animation0D084Indices[20] = {
#include "assets/kyle_800102_animation_0D084_indices.inc"
};

static AnimationSet _gKyle800102Animation0D084 = {
    _gKyle800102Animation0D084Records,
    _gKyle800102Animation0D084Indices,
    { NULL, _gKyle800102Animation0D084Bank1, NULL, NULL, _gKyle800102Animation0D084Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0D648Bank1[10] = {
#include "assets/kyle_800102_animation_0D648_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0D648Bank4[139] = {
#include "assets/kyle_800102_animation_0D648_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0D648Records[180] = {
#include "assets/kyle_800102_animation_0D648_records.inc"
};

static u16 _gKyle800102Animation0D648Indices[20] = {
#include "assets/kyle_800102_animation_0D648_indices.inc"
};

static AnimationSet _gKyle800102Animation0D648 = {
    _gKyle800102Animation0D648Records,
    _gKyle800102Animation0D648Indices,
    { NULL, _gKyle800102Animation0D648Bank1, NULL, NULL, _gKyle800102Animation0D648Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0DCBCBank1[12] = {
#include "assets/kyle_800102_animation_0DCBC_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0DCBCBank4[158] = {
#include "assets/kyle_800102_animation_0DCBC_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0DCBCRecords[199] = {
#include "assets/kyle_800102_animation_0DCBC_records.inc"
};

static u16 _gKyle800102Animation0DCBCIndices[20] = {
#include "assets/kyle_800102_animation_0DCBC_indices.inc"
};

static AnimationSet _gKyle800102Animation0DCBC = {
    _gKyle800102Animation0DCBCRecords,
    _gKyle800102Animation0DCBCIndices,
    { NULL, _gKyle800102Animation0DCBCBank1, NULL, NULL, _gKyle800102Animation0DCBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0E550Bank1[11] = {
#include "assets/kyle_800102_animation_0E550_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0E550Bank4[206] = {
#include "assets/kyle_800102_animation_0E550_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0E550Records[290] = {
#include "assets/kyle_800102_animation_0E550_records.inc"
};

static u16 _gKyle800102Animation0E550Indices[20] = {
#include "assets/kyle_800102_animation_0E550_indices.inc"
};

static AnimationSet _gKyle800102Animation0E550 = {
    _gKyle800102Animation0E550Records,
    _gKyle800102Animation0E550Indices,
    { NULL, _gKyle800102Animation0E550Bank1, NULL, NULL, _gKyle800102Animation0E550Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0ECA4Bank1[12] = {
#include "assets/kyle_800102_animation_0ECA4_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0ECA4Bank4[165] = {
#include "assets/kyle_800102_animation_0ECA4_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0ECA4Records[248] = {
#include "assets/kyle_800102_animation_0ECA4_records.inc"
};

static u16 _gKyle800102Animation0ECA4Indices[20] = {
#include "assets/kyle_800102_animation_0ECA4_indices.inc"
};

static AnimationSet _gKyle800102Animation0ECA4 = {
    _gKyle800102Animation0ECA4Records,
    _gKyle800102Animation0ECA4Indices,
    { NULL, _gKyle800102Animation0ECA4Bank1, NULL, NULL, _gKyle800102Animation0ECA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800102Animation0F850Bank1[17] = {
#include "assets/kyle_800102_animation_0F850_bank1.inc"
};

static AnimationPackedRotation _gKyle800102Animation0F850Bank4[298] = {
#include "assets/kyle_800102_animation_0F850_bank4.inc"
};

static AnimationRecord _gKyle800102Animation0F850Records[378] = {
#include "assets/kyle_800102_animation_0F850_records.inc"
};

static u16 _gKyle800102Animation0F850Indices[20] = {
#include "assets/kyle_800102_animation_0F850_indices.inc"
};

static AnimationSet _gKyle800102Animation0F850 = {
    _gKyle800102Animation0F850Records,
    _gKyle800102Animation0F850Indices,
    { NULL, _gKyle800102Animation0F850Bank1, NULL, NULL, _gKyle800102Animation0F850Bank4, NULL, NULL, NULL },
};

AnimationBank D_kyle_800102_801772E8 = { { {
    NULL,
    &_gKyle800102Animation070F8,
    &_gKyle800102Animation0E550,
    &_gKyle800102Animation0ECA4,
    &_gKyle800102Animation0F850,
    &_gKyle800102Animation081E4,
    &_gKyle800102Animation08AB8,
    &_gKyle800102Animation0D648,
    &_gKyle800102Animation0DCBC,
    &_gKyle800102Animation0AEB0,
    &_gKyle800102Animation0D084,
    &_gKyle800102Animation070F8,
    &_gKyle800102Animation0C030,
    &_gKyle800102Animation0B620,
    &_gKyle800102Animation0CCD8,
    &_gKyle800102Animation070F8,
    &_gKyle800102Animation08F78,
    &_gKyle800102Animation0951C,
    &_gKyle800102Animation09A8C,
    &_gKyle800102Animation07978,
    &_gKyle800102Animation0CCD8,
    &_gKyle800102Animation070F8,
    &_gKyle800102Animation070F8,
    &_gKyle800102Animation0A298,
    &_gKyle800102Animation070F8,
    &_gKyle800102Animation070F8,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gKyle800102Animation0A768,
    &_gKyle800102Animation0ACC0,
    &_gKyle800102Animation0D648,
    &_gKyle800102Animation0DCBC,
    &_gKyle800102Animation070F8,
    &_gKyle800102Animation070F8,
    &_gKyle800102Animation070F8,
    &_gKyle800102Animation070F8,
    &_gKyle800102Animation070F8,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
} } };

SVECTOR gGrenadeShellMuzzleOffsets[2] = {
    { 0, 0x1E0, 0x80, 0 },
    { 0, 0x220, 0x80, 0 },
};

u16 gGrenadeShellBlastRadii[4] = { 0x1F4, 0x4B0, 0x7D0, 0 };

u8 gGrenadeShellSpeeds[4] = { 0x0C, 0x08, 0, 0 };

static TmdBone _gKyle800102Model0FA78Skeleton[1] = {
#include "assets/kyle_800102_model_0FA78_skeleton.inc"
};

static u32 _gKyle800102Model0FA78PartVerts[1] = {
#include "assets/kyle_800102_model_0FA78_partVerts.inc"
};

static SVECTOR _gKyle800102Model0FA78Verts[8] = {
#include "assets/kyle_800102_model_0FA78_verts.inc"
};

static SVECTOR _gKyle800102Model0FA78Normals[8] = {
#include "assets/kyle_800102_model_0FA78_normals.inc"
};

static u32 _gKyle800102Model0FA78Stream[48] = {
#include "assets/kyle_800102_model_0FA78_stream.inc"
};

TmdSource D_kyle_800102_801775A8 = {
    0,
    312,
    0,
    1,
    _gKyle800102Model0FA78PartVerts,
    _gKyle800102Model0FA78Verts,
    _gKyle800102Model0FA78Normals,
    _gKyle800102Model0FA78Skeleton,
    _gKyle800102Model0FA78Stream,
};
