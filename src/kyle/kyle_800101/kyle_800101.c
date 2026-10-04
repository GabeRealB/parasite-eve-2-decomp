#include <psyq/sys/types.h>

#include "types.h"

#include "main/tmd_types.h"
#include "gameplay/animation.h"

static TmdBone _gKyle800101KyleMadiganBodySkeleton[20] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

static u32 _gKyle800101KyleMadiganBodyPartVerts[20] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

static SVECTOR _gKyle800101KyleMadiganBodyVerts[300] = {
#include "assets/kyle_madigan_body_verts.inc"
};

static SVECTOR _gKyle800101KyleMadiganBodyNormals[298] = {
#include "assets/kyle_madigan_body_normals.inc"
};

static u32 _gKyle800101KyleMadiganBodyStream[3412] = {
#include "assets/kyle_madigan_body_stream.inc"
};

TmdSource D_kyle_800101_8016C594 = {
    0,
    18224,
    5696,
    20,
    _gKyle800101KyleMadiganBodyPartVerts,
    _gKyle800101KyleMadiganBodyVerts,
    _gKyle800101KyleMadiganBodyNormals,
    _gKyle800101KyleMadiganBodySkeleton,
    _gKyle800101KyleMadiganBodyStream,
};

static TmdBone _gKyle800101KyleMadiganHandRightSkeleton[1] = {
#include "assets/kyle_madigan_hand_right_skeleton.inc"
};

static u32 _gKyle800101KyleMadiganHandRightPartVerts[1] = {
#include "assets/kyle_madigan_hand_right_partVerts.inc"
};

static SVECTOR _gKyle800101KyleMadiganHandRightVerts[23] = {
#include "assets/kyle_madigan_hand_right_verts.inc"
};

static SVECTOR _gKyle800101KyleMadiganHandRightNormals[23] = {
#include "assets/kyle_madigan_hand_right_normals.inc"
};

static u32 _gKyle800101KyleMadiganHandRightStream[166] = {
#include "assets/kyle_madigan_hand_right_stream.inc"
};

TmdSource D_kyle_800101_8016C9E8 = {
    0,
    1148,
    0,
    1,
    _gKyle800101KyleMadiganHandRightPartVerts,
    _gKyle800101KyleMadiganHandRightVerts,
    _gKyle800101KyleMadiganHandRightNormals,
    _gKyle800101KyleMadiganHandRightSkeleton,
    _gKyle800101KyleMadiganHandRightStream,
};

static TmdBone _gKyle800101Model05174Skeleton[1] = {
#include "assets/kyle_800101_model_05174_skeleton.inc"
};

static u32 _gKyle800101Model05174PartVerts[1] = {
#include "assets/kyle_800101_model_05174_partVerts.inc"
};

static SVECTOR _gKyle800101Model05174Verts[27] = {
#include "assets/kyle_800101_model_05174_verts.inc"
};

static SVECTOR _gKyle800101Model05174Normals[27] = {
#include "assets/kyle_800101_model_05174_normals.inc"
};

static u32 _gKyle800101Model05174Stream[189] = {
#include "assets/kyle_800101_model_05174_stream.inc"
};

TmdSource D_kyle_800101_8016CED8 = {
    0,
    1328,
    0,
    1,
    _gKyle800101Model05174PartVerts,
    _gKyle800101Model05174Verts,
    _gKyle800101Model05174Normals,
    _gKyle800101Model05174Skeleton,
    _gKyle800101Model05174Stream,
};

static TmdBone _gKyle800101KyleMadiganLeftSkeleton[1] = {
#include "assets/kyle_madigan_left_skeleton.inc"
};

static u32 _gKyle800101KyleMadiganLeftPartVerts[1] = {
#include "assets/kyle_madigan_left_partVerts.inc"
};

static SVECTOR _gKyle800101KyleMadiganLeftVerts[23] = {
#include "assets/kyle_madigan_left_verts.inc"
};

static SVECTOR _gKyle800101KyleMadiganLeftNormals[23] = {
#include "assets/kyle_madigan_left_normals.inc"
};

static u32 _gKyle800101KyleMadiganLeftStream[166] = {
#include "assets/kyle_madigan_left_stream.inc"
};

TmdSource D_kyle_800101_8016D32C = {
    0,
    1148,
    0,
    1,
    _gKyle800101KyleMadiganLeftPartVerts,
    _gKyle800101KyleMadiganLeftVerts,
    _gKyle800101KyleMadiganLeftNormals,
    _gKyle800101KyleMadiganLeftSkeleton,
    _gKyle800101KyleMadiganLeftStream,
};

static TmdBone _gKyle800101KyleMadiganHandLeftSkeleton[1] = {
#include "assets/kyle_madigan_hand_left_skeleton.inc"
};

static u32 _gKyle800101KyleMadiganHandLeftPartVerts[1] = {
#include "assets/kyle_madigan_hand_left_partVerts.inc"
};

static SVECTOR _gKyle800101KyleMadiganHandLeftVerts[27] = {
#include "assets/kyle_madigan_hand_left_verts.inc"
};

static SVECTOR _gKyle800101KyleMadiganHandLeftNormals[27] = {
#include "assets/kyle_madigan_hand_left_normals.inc"
};

static u32 _gKyle800101KyleMadiganHandLeftStream[189] = {
#include "assets/kyle_madigan_hand_left_stream.inc"
};

TmdSource D_kyle_800101_8016D81C = {
    0,
    1328,
    0,
    1,
    _gKyle800101KyleMadiganHandLeftPartVerts,
    _gKyle800101KyleMadiganHandLeftVerts,
    _gKyle800101KyleMadiganHandLeftNormals,
    _gKyle800101KyleMadiganHandLeftSkeleton,
    _gKyle800101KyleMadiganHandLeftStream,
};

static TmdBone _gKyle800101KyleMadiganGunSkeleton[1] = {
#include "assets/kyle_madigan_gun_skeleton.inc"
};

static u32 _gKyle800101KyleMadiganGunPartVerts[1] = {
#include "assets/kyle_madigan_gun_partVerts.inc"
};

static SVECTOR _gKyle800101KyleMadiganGunVerts[22] = {
#include "assets/kyle_madigan_gun_verts.inc"
};

static SVECTOR _gKyle800101KyleMadiganGunNormals[24] = {
#include "assets/kyle_madigan_gun_normals.inc"
};

static u32 _gKyle800101KyleMadiganGunStream[162] = {
#include "assets/kyle_madigan_gun_stream.inc"
};

TmdSource D_kyle_800101_8016DC60 = {
    0,
    1108,
    0,
    1,
    _gKyle800101KyleMadiganGunPartVerts,
    _gKyle800101KyleMadiganGunVerts,
    _gKyle800101KyleMadiganGunNormals,
    _gKyle800101KyleMadiganGunSkeleton,
    _gKyle800101KyleMadiganGunStream,
};

static AnimationPackedPose _gKyle800101Animation0641CBank1[2] = {
#include "assets/kyle_800101_animation_0641C_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0641CBank4[24] = {
#include "assets/kyle_800101_animation_0641C_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0641CRecords[90] = {
#include "assets/kyle_800101_animation_0641C_records.inc"
};

static u16 _gKyle800101Animation0641CIndices[20] = {
#include "assets/kyle_800101_animation_0641C_indices.inc"
};

static AnimationSet _gKyle800101Animation0641C = {
    _gKyle800101Animation0641CRecords,
    _gKyle800101Animation0641CIndices,
    { NULL, _gKyle800101Animation0641CBank1, NULL, NULL, _gKyle800101Animation0641CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation06C88Bank1[15] = {
#include "assets/kyle_800101_animation_06C88_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation06C88Bank4[171] = {
#include "assets/kyle_800101_animation_06C88_bank4.inc"
};

static AnimationRecord _gKyle800101Animation06C88Records[303] = {
#include "assets/kyle_800101_animation_06C88_records.inc"
};

static u16 _gKyle800101Animation06C88Indices[20] = {
#include "assets/kyle_800101_animation_06C88_indices.inc"
};

static AnimationSet _gKyle800101Animation06C88 = {
    _gKyle800101Animation06C88Records,
    _gKyle800101Animation06C88Indices,
    { NULL, _gKyle800101Animation06C88Bank1, NULL, NULL, _gKyle800101Animation06C88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0755CBank1[16] = {
#include "assets/kyle_800101_animation_0755C_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0755CBank4[182] = {
#include "assets/kyle_800101_animation_0755C_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0755CRecords[315] = {
#include "assets/kyle_800101_animation_0755C_records.inc"
};

static u16 _gKyle800101Animation0755CIndices[20] = {
#include "assets/kyle_800101_animation_0755C_indices.inc"
};

static AnimationSet _gKyle800101Animation0755C = {
    _gKyle800101Animation0755CRecords,
    _gKyle800101Animation0755CIndices,
    { NULL, _gKyle800101Animation0755CBank1, NULL, NULL, _gKyle800101Animation0755CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation07A1CBank1[7] = {
#include "assets/kyle_800101_animation_07A1C_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation07A1CBank4[105] = {
#include "assets/kyle_800101_animation_07A1C_bank4.inc"
};

static AnimationRecord _gKyle800101Animation07A1CRecords[158] = {
#include "assets/kyle_800101_animation_07A1C_records.inc"
};

static u16 _gKyle800101Animation07A1CIndices[20] = {
#include "assets/kyle_800101_animation_07A1C_indices.inc"
};

static AnimationSet _gKyle800101Animation07A1C = {
    _gKyle800101Animation07A1CRecords,
    _gKyle800101Animation07A1CIndices,
    { NULL, _gKyle800101Animation07A1CBank1, NULL, NULL, _gKyle800101Animation07A1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation07FC0Bank1[10] = {
#include "assets/kyle_800101_animation_07FC0_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation07FC0Bank4[131] = {
#include "assets/kyle_800101_animation_07FC0_bank4.inc"
};

static AnimationRecord _gKyle800101Animation07FC0Records[180] = {
#include "assets/kyle_800101_animation_07FC0_records.inc"
};

static u16 _gKyle800101Animation07FC0Indices[20] = {
#include "assets/kyle_800101_animation_07FC0_indices.inc"
};

static AnimationSet _gKyle800101Animation07FC0 = {
    _gKyle800101Animation07FC0Records,
    _gKyle800101Animation07FC0Indices,
    { NULL, _gKyle800101Animation07FC0Bank1, NULL, NULL, _gKyle800101Animation07FC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation08530Bank1[11] = {
#include "assets/kyle_800101_animation_08530_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation08530Bank4[127] = {
#include "assets/kyle_800101_animation_08530_bank4.inc"
};

static AnimationRecord _gKyle800101Animation08530Records[168] = {
#include "assets/kyle_800101_animation_08530_records.inc"
};

static u16 _gKyle800101Animation08530Indices[20] = {
#include "assets/kyle_800101_animation_08530_indices.inc"
};

static AnimationSet _gKyle800101Animation08530 = {
    _gKyle800101Animation08530Records,
    _gKyle800101Animation08530Indices,
    { NULL, _gKyle800101Animation08530Bank1, NULL, NULL, _gKyle800101Animation08530Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation08D3CBank1[10] = {
#include "assets/kyle_800101_animation_08D3C_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation08D3CBank4[181] = {
#include "assets/kyle_800101_animation_08D3C_bank4.inc"
};

static AnimationRecord _gKyle800101Animation08D3CRecords[284] = {
#include "assets/kyle_800101_animation_08D3C_records.inc"
};

static u16 _gKyle800101Animation08D3CIndices[20] = {
#include "assets/kyle_800101_animation_08D3C_indices.inc"
};

static AnimationSet _gKyle800101Animation08D3C = {
    _gKyle800101Animation08D3CRecords,
    _gKyle800101Animation08D3CIndices,
    { NULL, _gKyle800101Animation08D3CBank1, NULL, NULL, _gKyle800101Animation08D3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0920CBank1[9] = {
#include "assets/kyle_800101_animation_0920C_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0920CBank4[113] = {
#include "assets/kyle_800101_animation_0920C_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0920CRecords[148] = {
#include "assets/kyle_800101_animation_0920C_records.inc"
};

static u16 _gKyle800101Animation0920CIndices[20] = {
#include "assets/kyle_800101_animation_0920C_indices.inc"
};

static AnimationSet _gKyle800101Animation0920C = {
    _gKyle800101Animation0920CRecords,
    _gKyle800101Animation0920CIndices,
    { NULL, _gKyle800101Animation0920CBank1, NULL, NULL, _gKyle800101Animation0920CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation09764Bank1[8] = {
#include "assets/kyle_800101_animation_09764_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation09764Bank4[129] = {
#include "assets/kyle_800101_animation_09764_bank4.inc"
};

static AnimationRecord _gKyle800101Animation09764Records[169] = {
#include "assets/kyle_800101_animation_09764_records.inc"
};

static u16 _gKyle800101Animation09764Indices[20] = {
#include "assets/kyle_800101_animation_09764_indices.inc"
};

static AnimationSet _gKyle800101Animation09764 = {
    _gKyle800101Animation09764Records,
    _gKyle800101Animation09764Indices,
    { NULL, _gKyle800101Animation09764Bank1, NULL, NULL, _gKyle800101Animation09764Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation09958Bank1[2] = {
#include "assets/kyle_800101_animation_09958_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation09958Bank4[19] = {
#include "assets/kyle_800101_animation_09958_bank4.inc"
};

static AnimationRecord _gKyle800101Animation09958Records[80] = {
#include "assets/kyle_800101_animation_09958_records.inc"
};

static u16 _gKyle800101Animation09958Indices[20] = {
#include "assets/kyle_800101_animation_09958_indices.inc"
};

static AnimationSet _gKyle800101Animation09958 = {
    _gKyle800101Animation09958Records,
    _gKyle800101Animation09958Indices,
    { NULL, _gKyle800101Animation09958Bank1, NULL, NULL, _gKyle800101Animation09958Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation09B48Bank1[2] = {
#include "assets/kyle_800101_animation_09B48_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation09B48Bank4[18] = {
#include "assets/kyle_800101_animation_09B48_bank4.inc"
};

static AnimationRecord _gKyle800101Animation09B48Records[80] = {
#include "assets/kyle_800101_animation_09B48_records.inc"
};

static u16 _gKyle800101Animation09B48Indices[20] = {
#include "assets/kyle_800101_animation_09B48_indices.inc"
};

static AnimationSet _gKyle800101Animation09B48 = {
    _gKyle800101Animation09B48Records,
    _gKyle800101Animation09B48Indices,
    { NULL, _gKyle800101Animation09B48Bank1, NULL, NULL, _gKyle800101Animation09B48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0A2C4Bank1[12] = {
#include "assets/kyle_800101_animation_0A2C4_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0A2C4Bank4[159] = {
#include "assets/kyle_800101_animation_0A2C4_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0A2C4Records[264] = {
#include "assets/kyle_800101_animation_0A2C4_records.inc"
};

static u16 _gKyle800101Animation0A2C4Indices[20] = {
#include "assets/kyle_800101_animation_0A2C4_indices.inc"
};

static AnimationSet _gKyle800101Animation0A2C4 = {
    _gKyle800101Animation0A2C4Records,
    _gKyle800101Animation0A2C4Indices,
    { NULL, _gKyle800101Animation0A2C4Bank1, NULL, NULL, _gKyle800101Animation0A2C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0AC7CBank1[18] = {
#include "assets/kyle_800101_animation_0AC7C_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0AC7CBank4[234] = {
#include "assets/kyle_800101_animation_0AC7C_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0AC7CRecords[314] = {
#include "assets/kyle_800101_animation_0AC7C_records.inc"
};

static u16 _gKyle800101Animation0AC7CIndices[20] = {
#include "assets/kyle_800101_animation_0AC7C_indices.inc"
};

static AnimationSet _gKyle800101Animation0AC7C = {
    _gKyle800101Animation0AC7CRecords,
    _gKyle800101Animation0AC7CIndices,
    { NULL, _gKyle800101Animation0AC7CBank1, NULL, NULL, _gKyle800101Animation0AC7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0B6F0Bank1[18] = {
#include "assets/kyle_800101_animation_0B6F0_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0B6F0Bank4[258] = {
#include "assets/kyle_800101_animation_0B6F0_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0B6F0Records[337] = {
#include "assets/kyle_800101_animation_0B6F0_records.inc"
};

static u16 _gKyle800101Animation0B6F0Indices[20] = {
#include "assets/kyle_800101_animation_0B6F0_indices.inc"
};

static AnimationSet _gKyle800101Animation0B6F0 = {
    _gKyle800101Animation0B6F0Records,
    _gKyle800101Animation0B6F0Indices,
    { NULL, _gKyle800101Animation0B6F0Bank1, NULL, NULL, _gKyle800101Animation0B6F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0B968Bank1[2] = {
#include "assets/kyle_800101_animation_0B968_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0B968Bank4[50] = {
#include "assets/kyle_800101_animation_0B968_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0B968Records[82] = {
#include "assets/kyle_800101_animation_0B968_records.inc"
};

static u16 _gKyle800101Animation0B968Indices[20] = {
#include "assets/kyle_800101_animation_0B968_indices.inc"
};

static AnimationSet _gKyle800101Animation0B968 = {
    _gKyle800101Animation0B968Records,
    _gKyle800101Animation0B968Indices,
    { NULL, _gKyle800101Animation0B968Bank1, NULL, NULL, _gKyle800101Animation0B968Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0BD70Bank1[10] = {
#include "assets/kyle_800101_animation_0BD70_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0BD70Bank4[85] = {
#include "assets/kyle_800101_animation_0BD70_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0BD70Records[123] = {
#include "assets/kyle_800101_animation_0BD70_records.inc"
};

static u16 _gKyle800101Animation0BD70Indices[20] = {
#include "assets/kyle_800101_animation_0BD70_indices.inc"
};

static AnimationSet _gKyle800101Animation0BD70 = {
    _gKyle800101Animation0BD70Records,
    _gKyle800101Animation0BD70Indices,
    { NULL, _gKyle800101Animation0BD70Bank1, NULL, NULL, _gKyle800101Animation0BD70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0C24CBank1[8] = {
#include "assets/kyle_800101_animation_0C24C_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0C24CBank4[114] = {
#include "assets/kyle_800101_animation_0C24C_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0C24CRecords[153] = {
#include "assets/kyle_800101_animation_0C24C_records.inc"
};

static u16 _gKyle800101Animation0C24CIndices[20] = {
#include "assets/kyle_800101_animation_0C24C_indices.inc"
};

static AnimationSet _gKyle800101Animation0C24C = {
    _gKyle800101Animation0C24CRecords,
    _gKyle800101Animation0C24CIndices,
    { NULL, _gKyle800101Animation0C24CBank1, NULL, NULL, _gKyle800101Animation0C24CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0CACCBank1[11] = {
#include "assets/kyle_800101_animation_0CACC_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0CACCBank4[201] = {
#include "assets/kyle_800101_animation_0CACC_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0CACCRecords[290] = {
#include "assets/kyle_800101_animation_0CACC_records.inc"
};

static u16 _gKyle800101Animation0CACCIndices[20] = {
#include "assets/kyle_800101_animation_0CACC_indices.inc"
};

static AnimationSet _gKyle800101Animation0CACC = {
    _gKyle800101Animation0CACCRecords,
    _gKyle800101Animation0CACCIndices,
    { NULL, _gKyle800101Animation0CACCBank1, NULL, NULL, _gKyle800101Animation0CACCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0D4D0Bank1[17] = {
#include "assets/kyle_800101_animation_0D4D0_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0D4D0Bank4[246] = {
#include "assets/kyle_800101_animation_0D4D0_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0D4D0Records[324] = {
#include "assets/kyle_800101_animation_0D4D0_records.inc"
};

static u16 _gKyle800101Animation0D4D0Indices[20] = {
#include "assets/kyle_800101_animation_0D4D0_indices.inc"
};

static AnimationSet _gKyle800101Animation0D4D0 = {
    _gKyle800101Animation0D4D0Records,
    _gKyle800101Animation0D4D0Indices,
    { NULL, _gKyle800101Animation0D4D0Bank1, NULL, NULL, _gKyle800101Animation0D4D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gKyle800101Animation0DC38Bank1[12] = {
#include "assets/kyle_800101_animation_0DC38_bank1.inc"
};

static AnimationPackedRotation _gKyle800101Animation0DC38Bank4[167] = {
#include "assets/kyle_800101_animation_0DC38_bank4.inc"
};

static AnimationRecord _gKyle800101Animation0DC38Records[251] = {
#include "assets/kyle_800101_animation_0DC38_records.inc"
};

static u16 _gKyle800101Animation0DC38Indices[20] = {
#include "assets/kyle_800101_animation_0DC38_indices.inc"
};

static AnimationSet _gKyle800101Animation0DC38 = {
    _gKyle800101Animation0DC38Records,
    _gKyle800101Animation0DC38Indices,
    { NULL, _gKyle800101Animation0DC38Bank1, NULL, NULL, _gKyle800101Animation0DC38Bank4, NULL, NULL, NULL },
};

AnimationBank D_kyle_800101_801756D0 = { { {
    NULL,
    &_gKyle800101Animation0641C,
    &_gKyle800101Animation0CACC,
    &_gKyle800101Animation0DC38,
    &_gKyle800101Animation0D4D0,
    &_gKyle800101Animation06C88,
    &_gKyle800101Animation0755C,
    &_gKyle800101Animation0BD70,
    &_gKyle800101Animation0C24C,
    &_gKyle800101Animation09958,
    &_gKyle800101Animation0B968,
    &_gKyle800101Animation0641C,
    &_gKyle800101Animation0AC7C,
    &_gKyle800101Animation0A2C4,
    &_gKyle800101Animation0B6F0,
    &_gKyle800101Animation0641C,
    &_gKyle800101Animation07A1C,
    &_gKyle800101Animation07FC0,
    &_gKyle800101Animation08530,
    &_gKyle800101Animation0CACC,
    &_gKyle800101Animation0B6F0,
    &_gKyle800101Animation0641C,
    &_gKyle800101Animation0641C,
    &_gKyle800101Animation08D3C,
    &_gKyle800101Animation0641C,
    &_gKyle800101Animation0641C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gKyle800101Animation0920C,
    &_gKyle800101Animation09764,
    &_gKyle800101Animation0BD70,
    &_gKyle800101Animation0C24C,
    &_gKyle800101Animation0641C,
    &_gKyle800101Animation0641C,
    &_gKyle800101Animation0641C,
    &_gKyle800101Animation0641C,
    &_gKyle800101Animation0641C,
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
