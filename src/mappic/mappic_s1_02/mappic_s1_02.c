#include <psyq/sys/types.h>

#include "types.h"

#include "main/tmd_types.h"

static TmdBone _gMappicS102MappicS202Model00C4800070Skeleton[1] = {
#include "assets/mappic_s2_02_model_00C48_00070_skeleton.inc"
};

static u32 _gMappicS102MappicS202Model00C4800070PartVerts[1] = {
#include "assets/mappic_s2_02_model_00C48_00070_partVerts.inc"
};

static SVECTOR _gMappicS102MappicS202Model00C4800070Verts[4] = {
#include "assets/mappic_s2_02_model_00C48_00070_verts.inc"
};

static u32 _gMappicS102MappicS202Model00C4800070Stream[9] = {
#include "assets/mappic_s2_02_model_00C48_00070_stream.inc"
};

TmdSource D_mappic_s1_02_8012EFA0 = {
    0,
    24,
    0,
    1,
    _gMappicS102MappicS202Model00C4800070PartVerts,
    _gMappicS102MappicS202Model00C4800070Verts,
    _gMappicS102MappicS202Model00C4800070Stream,
    _gMappicS102MappicS202Model00C4800070Skeleton,
    _gMappicS102MappicS202Model00C4800070Stream,
};
