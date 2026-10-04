#include <psyq/sys/types.h>

#include "types.h"

#include "main/tmd_types.h"
#include "gameplay/animation.h"

static TmdBone _gUnused85Model0016CSkeleton[1] = {
#include "assets/unused_85_model_0016C_skeleton.inc"
};

static u32 _gUnused85Model0016CPartVerts[1] = {
#include "assets/unused_85_model_0016C_partVerts.inc"
};

static SVECTOR _gUnused85Model0016CVerts[20] = {
#include "assets/unused_85_model_0016C_verts.inc"
};

static SVECTOR _gUnused85Model0016CNormals[20] = {
#include "assets/unused_85_model_0016C_normals.inc"
};

static u32 _gUnused85Model0016CStream[132] = {
#include "assets/unused_85_model_0016C_stream.inc"
};

TmdSource D_unused_85_8011D53C = {
    0,
    936,
    0,
    1,
    _gUnused85Model0016CPartVerts,
    _gUnused85Model0016CVerts,
    _gUnused85Model0016CNormals,
    _gUnused85Model0016CSkeleton,
    _gUnused85Model0016CStream,
};

static AnimationPackedPose _gUnused85Animation00530Bank1[2] = {
#include "assets/unused_85_animation_00530_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation00530Bank4[8] = {
#include "assets/unused_85_animation_00530_bank4.inc"
};

static AnimationRecord _gUnused85Animation00530Records[76] = {
#include "assets/unused_85_animation_00530_records.inc"
};

static u16 _gUnused85Animation00530Indices[20] = {
#include "assets/unused_85_animation_00530_indices.inc"
};

static AnimationSet _gUnused85Animation00530 = {
    _gUnused85Animation00530Records,
    _gUnused85Animation00530Indices,
    { NULL, _gUnused85Animation00530Bank1, NULL, NULL, _gUnused85Animation00530Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation00BD4Bank1[12] = {
#include "assets/unused_85_animation_00BD4_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation00BD4Bank4[151] = {
#include "assets/unused_85_animation_00BD4_bank4.inc"
};

static AnimationRecord _gUnused85Animation00BD4Records[218] = {
#include "assets/unused_85_animation_00BD4_records.inc"
};

static u16 _gUnused85Animation00BD4Indices[20] = {
#include "assets/unused_85_animation_00BD4_indices.inc"
};

static AnimationSet _gUnused85Animation00BD4 = {
    _gUnused85Animation00BD4Records,
    _gUnused85Animation00BD4Indices,
    { NULL, _gUnused85Animation00BD4Bank1, NULL, NULL, _gUnused85Animation00BD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation01520Bank1[16] = {
#include "assets/unused_85_animation_01520_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation01520Bank4[221] = {
#include "assets/unused_85_animation_01520_bank4.inc"
};

static AnimationRecord _gUnused85Animation01520Records[306] = {
#include "assets/unused_85_animation_01520_records.inc"
};

static u16 _gUnused85Animation01520Indices[20] = {
#include "assets/unused_85_animation_01520_indices.inc"
};

static AnimationSet _gUnused85Animation01520 = {
    _gUnused85Animation01520Records,
    _gUnused85Animation01520Indices,
    { NULL, _gUnused85Animation01520Bank1, NULL, NULL, _gUnused85Animation01520Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation01DF8Bank1[19] = {
#include "assets/unused_85_animation_01DF8_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation01DF8Bank4[193] = {
#include "assets/unused_85_animation_01DF8_bank4.inc"
};

static AnimationRecord _gUnused85Animation01DF8Records[296] = {
#include "assets/unused_85_animation_01DF8_records.inc"
};

static u16 _gUnused85Animation01DF8Indices[20] = {
#include "assets/unused_85_animation_01DF8_indices.inc"
};

static AnimationSet _gUnused85Animation01DF8 = {
    _gUnused85Animation01DF8Records,
    _gUnused85Animation01DF8Indices,
    { NULL, _gUnused85Animation01DF8Bank1, NULL, NULL, _gUnused85Animation01DF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0252CBank1[13] = {
#include "assets/unused_85_animation_0252C_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0252CBank4[167] = {
#include "assets/unused_85_animation_0252C_bank4.inc"
};

static AnimationRecord _gUnused85Animation0252CRecords[235] = {
#include "assets/unused_85_animation_0252C_records.inc"
};

static u16 _gUnused85Animation0252CIndices[20] = {
#include "assets/unused_85_animation_0252C_indices.inc"
};

static AnimationSet _gUnused85Animation0252C = {
    _gUnused85Animation0252CRecords,
    _gUnused85Animation0252CIndices,
    { NULL, _gUnused85Animation0252CBank1, NULL, NULL, _gUnused85Animation0252CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation02D8CBank1[19] = {
#include "assets/unused_85_animation_02D8C_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation02D8CBank4[169] = {
#include "assets/unused_85_animation_02D8C_bank4.inc"
};

static AnimationRecord _gUnused85Animation02D8CRecords[290] = {
#include "assets/unused_85_animation_02D8C_records.inc"
};

static u16 _gUnused85Animation02D8CIndices[20] = {
#include "assets/unused_85_animation_02D8C_indices.inc"
};

static AnimationSet _gUnused85Animation02D8C = {
    _gUnused85Animation02D8CRecords,
    _gUnused85Animation02D8CIndices,
    { NULL, _gUnused85Animation02D8CBank1, NULL, NULL, _gUnused85Animation02D8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation035F0Bank1[19] = {
#include "assets/unused_85_animation_035F0_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation035F0Bank4[170] = {
#include "assets/unused_85_animation_035F0_bank4.inc"
};

static AnimationRecord _gUnused85Animation035F0Records[290] = {
#include "assets/unused_85_animation_035F0_records.inc"
};

static u16 _gUnused85Animation035F0Indices[20] = {
#include "assets/unused_85_animation_035F0_indices.inc"
};

static AnimationSet _gUnused85Animation035F0 = {
    _gUnused85Animation035F0Records,
    _gUnused85Animation035F0Indices,
    { NULL, _gUnused85Animation035F0Bank1, NULL, NULL, _gUnused85Animation035F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation03904Bank1[3] = {
#include "assets/unused_85_animation_03904_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation03904Bank4[69] = {
#include "assets/unused_85_animation_03904_bank4.inc"
};

static AnimationRecord _gUnused85Animation03904Records[99] = {
#include "assets/unused_85_animation_03904_records.inc"
};

static u16 _gUnused85Animation03904Indices[20] = {
#include "assets/unused_85_animation_03904_indices.inc"
};

static AnimationSet _gUnused85Animation03904 = {
    _gUnused85Animation03904Records,
    _gUnused85Animation03904Indices,
    { NULL, _gUnused85Animation03904Bank1, NULL, NULL, _gUnused85Animation03904Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation04060Bank1[14] = {
#include "assets/unused_85_animation_04060_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation04060Bank4[156] = {
#include "assets/unused_85_animation_04060_bank4.inc"
};

static AnimationRecord _gUnused85Animation04060Records[253] = {
#include "assets/unused_85_animation_04060_records.inc"
};

static u16 _gUnused85Animation04060Indices[20] = {
#include "assets/unused_85_animation_04060_indices.inc"
};

static AnimationSet _gUnused85Animation04060 = {
    _gUnused85Animation04060Records,
    _gUnused85Animation04060Indices,
    { NULL, _gUnused85Animation04060Bank1, NULL, NULL, _gUnused85Animation04060Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation047E8Bank1[16] = {
#include "assets/unused_85_animation_047E8_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation047E8Bank4[167] = {
#include "assets/unused_85_animation_047E8_bank4.inc"
};

static AnimationRecord _gUnused85Animation047E8Records[247] = {
#include "assets/unused_85_animation_047E8_records.inc"
};

static u16 _gUnused85Animation047E8Indices[20] = {
#include "assets/unused_85_animation_047E8_indices.inc"
};

static AnimationSet _gUnused85Animation047E8 = {
    _gUnused85Animation047E8Records,
    _gUnused85Animation047E8Indices,
    { NULL, _gUnused85Animation047E8Bank1, NULL, NULL, _gUnused85Animation047E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation04ABCBank1[6] = {
#include "assets/unused_85_animation_04ABC_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation04ABCBank4[52] = {
#include "assets/unused_85_animation_04ABC_bank4.inc"
};

static AnimationRecord _gUnused85Animation04ABCRecords[91] = {
#include "assets/unused_85_animation_04ABC_records.inc"
};

static u16 _gUnused85Animation04ABCIndices[20] = {
#include "assets/unused_85_animation_04ABC_indices.inc"
};

static AnimationSet _gUnused85Animation04ABC = {
    _gUnused85Animation04ABCRecords,
    _gUnused85Animation04ABCIndices,
    { NULL, _gUnused85Animation04ABCBank1, NULL, NULL, _gUnused85Animation04ABCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation04E4CBank1[7] = {
#include "assets/unused_85_animation_04E4C_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation04E4CBank4[73] = {
#include "assets/unused_85_animation_04E4C_bank4.inc"
};

static AnimationRecord _gUnused85Animation04E4CRecords[114] = {
#include "assets/unused_85_animation_04E4C_records.inc"
};

static u16 _gUnused85Animation04E4CIndices[20] = {
#include "assets/unused_85_animation_04E4C_indices.inc"
};

static AnimationSet _gUnused85Animation04E4C = {
    _gUnused85Animation04E4CRecords,
    _gUnused85Animation04E4CIndices,
    { NULL, _gUnused85Animation04E4CBank1, NULL, NULL, _gUnused85Animation04E4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation052D4Bank1[9] = {
#include "assets/unused_85_animation_052D4_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation052D4Bank4[104] = {
#include "assets/unused_85_animation_052D4_bank4.inc"
};

static AnimationRecord _gUnused85Animation052D4Records[139] = {
#include "assets/unused_85_animation_052D4_records.inc"
};

static u16 _gUnused85Animation052D4Indices[20] = {
#include "assets/unused_85_animation_052D4_indices.inc"
};

static AnimationSet _gUnused85Animation052D4 = {
    _gUnused85Animation052D4Records,
    _gUnused85Animation052D4Indices,
    { NULL, _gUnused85Animation052D4Bank1, NULL, NULL, _gUnused85Animation052D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation054D0Bank1[3] = {
#include "assets/unused_85_animation_054D0_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation054D0Bank4[22] = {
#include "assets/unused_85_animation_054D0_bank4.inc"
};

static AnimationRecord _gUnused85Animation054D0Records[76] = {
#include "assets/unused_85_animation_054D0_records.inc"
};

static u16 _gUnused85Animation054D0Indices[20] = {
#include "assets/unused_85_animation_054D0_indices.inc"
};

static AnimationSet _gUnused85Animation054D0 = {
    _gUnused85Animation054D0Records,
    _gUnused85Animation054D0Indices,
    { NULL, _gUnused85Animation054D0Bank1, NULL, NULL, _gUnused85Animation054D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation057A8Bank1[6] = {
#include "assets/unused_85_animation_057A8_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation057A8Bank4[57] = {
#include "assets/unused_85_animation_057A8_bank4.inc"
};

static AnimationRecord _gUnused85Animation057A8Records[87] = {
#include "assets/unused_85_animation_057A8_records.inc"
};

static u16 _gUnused85Animation057A8Indices[20] = {
#include "assets/unused_85_animation_057A8_indices.inc"
};

static AnimationSet _gUnused85Animation057A8 = {
    _gUnused85Animation057A8Records,
    _gUnused85Animation057A8Indices,
    { NULL, _gUnused85Animation057A8Bank1, NULL, NULL, _gUnused85Animation057A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation05A4CBank1[4] = {
#include "assets/unused_85_animation_05A4C_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation05A4CBank4[55] = {
#include "assets/unused_85_animation_05A4C_bank4.inc"
};

static AnimationRecord _gUnused85Animation05A4CRecords[82] = {
#include "assets/unused_85_animation_05A4C_records.inc"
};

static u16 _gUnused85Animation05A4CIndices[20] = {
#include "assets/unused_85_animation_05A4C_indices.inc"
};

static AnimationSet _gUnused85Animation05A4C = {
    _gUnused85Animation05A4CRecords,
    _gUnused85Animation05A4CIndices,
    { NULL, _gUnused85Animation05A4CBank1, NULL, NULL, _gUnused85Animation05A4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation05C4CBank1[3] = {
#include "assets/unused_85_animation_05C4C_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation05C4CBank4[23] = {
#include "assets/unused_85_animation_05C4C_bank4.inc"
};

static AnimationRecord _gUnused85Animation05C4CRecords[76] = {
#include "assets/unused_85_animation_05C4C_records.inc"
};

static u16 _gUnused85Animation05C4CIndices[20] = {
#include "assets/unused_85_animation_05C4C_indices.inc"
};

static AnimationSet _gUnused85Animation05C4C = {
    _gUnused85Animation05C4CRecords,
    _gUnused85Animation05C4CIndices,
    { NULL, _gUnused85Animation05C4CBank1, NULL, NULL, _gUnused85Animation05C4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation05FA0Bank1[8] = {
#include "assets/unused_85_animation_05FA0_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation05FA0Bank4[68] = {
#include "assets/unused_85_animation_05FA0_bank4.inc"
};

static AnimationRecord _gUnused85Animation05FA0Records[101] = {
#include "assets/unused_85_animation_05FA0_records.inc"
};

static u16 _gUnused85Animation05FA0Indices[20] = {
#include "assets/unused_85_animation_05FA0_indices.inc"
};

static AnimationSet _gUnused85Animation05FA0 = {
    _gUnused85Animation05FA0Records,
    _gUnused85Animation05FA0Indices,
    { NULL, _gUnused85Animation05FA0Bank1, NULL, NULL, _gUnused85Animation05FA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation06254Bank1[5] = {
#include "assets/unused_85_animation_06254_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation06254Bank4[55] = {
#include "assets/unused_85_animation_06254_bank4.inc"
};

static AnimationRecord _gUnused85Animation06254Records[83] = {
#include "assets/unused_85_animation_06254_records.inc"
};

static u16 _gUnused85Animation06254Indices[20] = {
#include "assets/unused_85_animation_06254_indices.inc"
};

static AnimationSet _gUnused85Animation06254 = {
    _gUnused85Animation06254Records,
    _gUnused85Animation06254Indices,
    { NULL, _gUnused85Animation06254Bank1, NULL, NULL, _gUnused85Animation06254Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation06574Bank1[6] = {
#include "assets/unused_85_animation_06574_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation06574Bank4[66] = {
#include "assets/unused_85_animation_06574_bank4.inc"
};

static AnimationRecord _gUnused85Animation06574Records[96] = {
#include "assets/unused_85_animation_06574_records.inc"
};

static u16 _gUnused85Animation06574Indices[20] = {
#include "assets/unused_85_animation_06574_indices.inc"
};

static AnimationSet _gUnused85Animation06574 = {
    _gUnused85Animation06574Records,
    _gUnused85Animation06574Indices,
    { NULL, _gUnused85Animation06574Bank1, NULL, NULL, _gUnused85Animation06574Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation06D38Bank1[18] = {
#include "assets/unused_85_animation_06D38_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation06D38Bank4[184] = {
#include "assets/unused_85_animation_06D38_bank4.inc"
};

static AnimationRecord _gUnused85Animation06D38Records[239] = {
#include "assets/unused_85_animation_06D38_records.inc"
};

static u16 _gUnused85Animation06D38Indices[20] = {
#include "assets/unused_85_animation_06D38_indices.inc"
};

static AnimationSet _gUnused85Animation06D38 = {
    _gUnused85Animation06D38Records,
    _gUnused85Animation06D38Indices,
    { NULL, _gUnused85Animation06D38Bank1, NULL, NULL, _gUnused85Animation06D38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation07FD0Bank1[29] = {
#include "assets/unused_85_animation_07FD0_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation07FD0Bank4[450] = {
#include "assets/unused_85_animation_07FD0_bank4.inc"
};

static AnimationRecord _gUnused85Animation07FD0Records[633] = {
#include "assets/unused_85_animation_07FD0_records.inc"
};

static u16 _gUnused85Animation07FD0Indices[20] = {
#include "assets/unused_85_animation_07FD0_indices.inc"
};

static AnimationSet _gUnused85Animation07FD0 = {
    _gUnused85Animation07FD0Records,
    _gUnused85Animation07FD0Indices,
    { NULL, _gUnused85Animation07FD0Bank1, NULL, NULL, _gUnused85Animation07FD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation08B48Bank1[12] = {
#include "assets/unused_85_animation_08B48_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation08B48Bank4[266] = {
#include "assets/unused_85_animation_08B48_bank4.inc"
};

static AnimationRecord _gUnused85Animation08B48Records[412] = {
#include "assets/unused_85_animation_08B48_records.inc"
};

static u16 _gUnused85Animation08B48Indices[20] = {
#include "assets/unused_85_animation_08B48_indices.inc"
};

static AnimationSet _gUnused85Animation08B48 = {
    _gUnused85Animation08B48Records,
    _gUnused85Animation08B48Indices,
    { NULL, _gUnused85Animation08B48Bank1, NULL, NULL, _gUnused85Animation08B48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation09264Bank1[9] = {
#include "assets/unused_85_animation_09264_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation09264Bank4[144] = {
#include "assets/unused_85_animation_09264_bank4.inc"
};

static AnimationRecord _gUnused85Animation09264Records[264] = {
#include "assets/unused_85_animation_09264_records.inc"
};

static u16 _gUnused85Animation09264Indices[20] = {
#include "assets/unused_85_animation_09264_indices.inc"
};

static AnimationSet _gUnused85Animation09264 = {
    _gUnused85Animation09264Records,
    _gUnused85Animation09264Indices,
    { NULL, _gUnused85Animation09264Bank1, NULL, NULL, _gUnused85Animation09264Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation096E4Bank1[6] = {
#include "assets/unused_85_animation_096E4_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation096E4Bank4[107] = {
#include "assets/unused_85_animation_096E4_bank4.inc"
};

static AnimationRecord _gUnused85Animation096E4Records[143] = {
#include "assets/unused_85_animation_096E4_records.inc"
};

static u16 _gUnused85Animation096E4Indices[20] = {
#include "assets/unused_85_animation_096E4_indices.inc"
};

static AnimationSet _gUnused85Animation096E4 = {
    _gUnused85Animation096E4Records,
    _gUnused85Animation096E4Indices,
    { NULL, _gUnused85Animation096E4Bank1, NULL, NULL, _gUnused85Animation096E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation098BCBank1[3] = {
#include "assets/unused_85_animation_098BC_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation098BCBank4[32] = {
#include "assets/unused_85_animation_098BC_bank4.inc"
};

static AnimationRecord _gUnused85Animation098BCRecords[57] = {
#include "assets/unused_85_animation_098BC_records.inc"
};

static u16 _gUnused85Animation098BCIndices[20] = {
#include "assets/unused_85_animation_098BC_indices.inc"
};

static AnimationSet _gUnused85Animation098BC = {
    _gUnused85Animation098BCRecords,
    _gUnused85Animation098BCIndices,
    { NULL, _gUnused85Animation098BCBank1, NULL, NULL, _gUnused85Animation098BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation09E20Bank1[11] = {
#include "assets/unused_85_animation_09E20_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation09E20Bank4[125] = {
#include "assets/unused_85_animation_09E20_bank4.inc"
};

static AnimationRecord _gUnused85Animation09E20Records[167] = {
#include "assets/unused_85_animation_09E20_records.inc"
};

static u16 _gUnused85Animation09E20Indices[20] = {
#include "assets/unused_85_animation_09E20_indices.inc"
};

static AnimationSet _gUnused85Animation09E20 = {
    _gUnused85Animation09E20Records,
    _gUnused85Animation09E20Indices,
    { NULL, _gUnused85Animation09E20Bank1, NULL, NULL, _gUnused85Animation09E20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0A014Bank1[3] = {
#include "assets/unused_85_animation_0A014_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0A014Bank4[20] = {
#include "assets/unused_85_animation_0A014_bank4.inc"
};

static AnimationRecord _gUnused85Animation0A014Records[76] = {
#include "assets/unused_85_animation_0A014_records.inc"
};

static u16 _gUnused85Animation0A014Indices[20] = {
#include "assets/unused_85_animation_0A014_indices.inc"
};

static AnimationSet _gUnused85Animation0A014 = {
    _gUnused85Animation0A014Records,
    _gUnused85Animation0A014Indices,
    { NULL, _gUnused85Animation0A014Bank1, NULL, NULL, _gUnused85Animation0A014Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0A494Bank1[8] = {
#include "assets/unused_85_animation_0A494_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0A494Bank4[105] = {
#include "assets/unused_85_animation_0A494_bank4.inc"
};

static AnimationRecord _gUnused85Animation0A494Records[139] = {
#include "assets/unused_85_animation_0A494_records.inc"
};

static u16 _gUnused85Animation0A494Indices[20] = {
#include "assets/unused_85_animation_0A494_indices.inc"
};

static AnimationSet _gUnused85Animation0A494 = {
    _gUnused85Animation0A494Records,
    _gUnused85Animation0A494Indices,
    { NULL, _gUnused85Animation0A494Bank1, NULL, NULL, _gUnused85Animation0A494Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0A670Bank1[2] = {
#include "assets/unused_85_animation_0A670_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0A670Bank4[17] = {
#include "assets/unused_85_animation_0A670_bank4.inc"
};

static AnimationRecord _gUnused85Animation0A670Records[76] = {
#include "assets/unused_85_animation_0A670_records.inc"
};

static u16 _gUnused85Animation0A670Indices[20] = {
#include "assets/unused_85_animation_0A670_indices.inc"
};

static AnimationSet _gUnused85Animation0A670 = {
    _gUnused85Animation0A670Records,
    _gUnused85Animation0A670Indices,
    { NULL, _gUnused85Animation0A670Bank1, NULL, NULL, _gUnused85Animation0A670Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0AD60Bank1[12] = {
#include "assets/unused_85_animation_0AD60_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0AD60Bank4[164] = {
#include "assets/unused_85_animation_0AD60_bank4.inc"
};

static AnimationRecord _gUnused85Animation0AD60Records[224] = {
#include "assets/unused_85_animation_0AD60_records.inc"
};

static u16 _gUnused85Animation0AD60Indices[20] = {
#include "assets/unused_85_animation_0AD60_indices.inc"
};

static AnimationSet _gUnused85Animation0AD60 = {
    _gUnused85Animation0AD60Records,
    _gUnused85Animation0AD60Indices,
    { NULL, _gUnused85Animation0AD60Bank1, NULL, NULL, _gUnused85Animation0AD60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0B94CBank1[19] = {
#include "assets/unused_85_animation_0B94C_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0B94CBank4[304] = {
#include "assets/unused_85_animation_0B94C_bank4.inc"
};

static AnimationRecord _gUnused85Animation0B94CRecords[382] = {
#include "assets/unused_85_animation_0B94C_records.inc"
};

static u16 _gUnused85Animation0B94CIndices[20] = {
#include "assets/unused_85_animation_0B94C_indices.inc"
};

static AnimationSet _gUnused85Animation0B94C = {
    _gUnused85Animation0B94CRecords,
    _gUnused85Animation0B94CIndices,
    { NULL, _gUnused85Animation0B94CBank1, NULL, NULL, _gUnused85Animation0B94CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0C274Bank1[15] = {
#include "assets/unused_85_animation_0C274_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0C274Bank4[231] = {
#include "assets/unused_85_animation_0C274_bank4.inc"
};

static AnimationRecord _gUnused85Animation0C274Records[290] = {
#include "assets/unused_85_animation_0C274_records.inc"
};

static u16 _gUnused85Animation0C274Indices[20] = {
#include "assets/unused_85_animation_0C274_indices.inc"
};

static AnimationSet _gUnused85Animation0C274 = {
    _gUnused85Animation0C274Records,
    _gUnused85Animation0C274Indices,
    { NULL, _gUnused85Animation0C274Bank1, NULL, NULL, _gUnused85Animation0C274Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0C644Bank1[6] = {
#include "assets/unused_85_animation_0C644_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0C644Bank4[81] = {
#include "assets/unused_85_animation_0C644_bank4.inc"
};

static AnimationRecord _gUnused85Animation0C644Records[125] = {
#include "assets/unused_85_animation_0C644_records.inc"
};

static u16 _gUnused85Animation0C644Indices[20] = {
#include "assets/unused_85_animation_0C644_indices.inc"
};

static AnimationSet _gUnused85Animation0C644 = {
    _gUnused85Animation0C644Records,
    _gUnused85Animation0C644Indices,
    { NULL, _gUnused85Animation0C644Bank1, NULL, NULL, _gUnused85Animation0C644Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0CCC0Bank1[11] = {
#include "assets/unused_85_animation_0CCC0_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0CCC0Bank4[161] = {
#include "assets/unused_85_animation_0CCC0_bank4.inc"
};

static AnimationRecord _gUnused85Animation0CCC0Records[201] = {
#include "assets/unused_85_animation_0CCC0_records.inc"
};

static u16 _gUnused85Animation0CCC0Indices[20] = {
#include "assets/unused_85_animation_0CCC0_indices.inc"
};

static AnimationSet _gUnused85Animation0CCC0 = {
    _gUnused85Animation0CCC0Records,
    _gUnused85Animation0CCC0Indices,
    { NULL, _gUnused85Animation0CCC0Bank1, NULL, NULL, _gUnused85Animation0CCC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gUnused85Animation0D28CBank1[10] = {
#include "assets/unused_85_animation_0D28C_bank1.inc"
};

static AnimationPackedRotation _gUnused85Animation0D28CBank4[145] = {
#include "assets/unused_85_animation_0D28C_bank4.inc"
};

static AnimationRecord _gUnused85Animation0D28CRecords[176] = {
#include "assets/unused_85_animation_0D28C_records.inc"
};

static u16 _gUnused85Animation0D28CIndices[20] = {
#include "assets/unused_85_animation_0D28C_indices.inc"
};

static AnimationSet _gUnused85Animation0D28C = {
    _gUnused85Animation0D28CRecords,
    _gUnused85Animation0D28CIndices,
    { NULL, _gUnused85Animation0D28CBank1, NULL, NULL, _gUnused85Animation0D28CBank4, NULL, NULL, NULL },
};

AnimationBank D_unused_85_8012A474 = { { {
    NULL,
    &_gUnused85Animation00530,
    &_gUnused85Animation01520,
    &_gUnused85Animation01DF8,
    &_gUnused85Animation0252C,
    &_gUnused85Animation02D8C,
    &_gUnused85Animation035F0,
    &_gUnused85Animation0CCC0,
    &_gUnused85Animation0D28C,
    &_gUnused85Animation0A670,
    &_gUnused85Animation0C644,
    &_gUnused85Animation0C644,
    &_gUnused85Animation0B94C,
    &_gUnused85Animation0AD60,
    &_gUnused85Animation0C274,
    &_gUnused85Animation0C274,
    &_gUnused85Animation06254,
    &_gUnused85Animation06574,
    &_gUnused85Animation06D38,
    &_gUnused85Animation00BD4,
    &_gUnused85Animation0C274,
    &_gUnused85Animation00530,
    &_gUnused85Animation00530,
    &_gUnused85Animation07FD0,
    &_gUnused85Animation09264,
    &_gUnused85Animation08B48,
    &_gUnused85Animation052D4,
    &_gUnused85Animation054D0,
    &_gUnused85Animation057A8,
    &_gUnused85Animation05A4C,
    &_gUnused85Animation05C4C,
    &_gUnused85Animation05FA0,
    &_gUnused85Animation096E4,
    &_gUnused85Animation098BC,
    &_gUnused85Animation096E4,
    &_gUnused85Animation098BC,
    &_gUnused85Animation04060,
    &_gUnused85Animation047E8,
    &_gUnused85Animation04E4C,
    &_gUnused85Animation04ABC,
    &_gUnused85Animation03904,
    &_gUnused85Animation00530,
    &_gUnused85Animation09E20,
    &_gUnused85Animation0A014,
    &_gUnused85Animation0A494,
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
