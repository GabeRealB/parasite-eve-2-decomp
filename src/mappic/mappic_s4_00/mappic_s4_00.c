#include <psyq/sys/types.h>

#include "types.h"

#include "main/tmd_types.h"

static TmdBone _gMappicS400Model0005C0008CMappicS400Skeleton[1] = {
#include "assets/mappic_s4_00_model_0005C_0008C_mappic_s4_00_skeleton.inc"
};

static u32 _gMappicS400Model0005C0008CMappicS400PartVerts[1] = {
#include "assets/mappic_s4_00_model_0005C_0008C_mappic_s4_00_partVerts.inc"
};

static SVECTOR _gMappicS400Model0005C0008CMappicS400Verts[6] = {
#include "assets/mappic_s4_00_model_0005C_0008C_mappic_s4_00_verts.inc"
};

static u32 _gMappicS400Model0005C0008CMappicS400Stream[12] = {
#include "assets/mappic_s4_00_model_0005C_0008C_mappic_s4_00_stream.inc"
};

TmdSource D_mappic_s4_00_8012EFBC = {
    0,
    48,
    0,
    1,
    _gMappicS400Model0005C0008CMappicS400PartVerts,
    _gMappicS400Model0005C0008CMappicS400Verts,
    _gMappicS400Model0005C0008CMappicS400Stream,
    _gMappicS400Model0005C0008CMappicS400Skeleton,
    _gMappicS400Model0005C0008CMappicS400Stream,
};

static TmdBone _gMappicS400Model00148Skeleton[1] = {
#include "assets/mappic_s4_00_model_00148_skeleton.inc"
};

static u32 _gMappicS400Model00148PartVerts[1] = {
#include "assets/mappic_s4_00_model_00148_partVerts.inc"
};

static SVECTOR _gMappicS400Model00148Verts[14] = {
#include "assets/mappic_s4_00_model_00148_verts.inc"
};

static u32 _gMappicS400Model00148Stream[24] = {
#include "assets/mappic_s4_00_model_00148_stream.inc"
};

TmdSource D_mappic_s4_00_8012F0D8 = {
    0,
    144,
    0,
    1,
    _gMappicS400Model00148PartVerts,
    _gMappicS400Model00148Verts,
    _gMappicS400Model00148Stream,
    _gMappicS400Model00148Skeleton,
    _gMappicS400Model00148Stream,
};
