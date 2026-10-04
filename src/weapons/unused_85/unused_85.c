#include <psyq/sys/types.h>

#include "types.h"

#include "main/tmd_types.h"

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
