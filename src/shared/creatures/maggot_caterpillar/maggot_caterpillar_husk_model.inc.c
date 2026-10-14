/* Each Maggot/Caterpillar carrier includes this at its model's data position.
 * Requires TmdSource, TmdBone, SVECTOR, POLY_GT3, POLY_GT4 and ARRAY_SIZE.
 */

enum {
    MAGGOT_CATERPILLAR_HUSK_HANDLERS_UNRESOLVED = 0,
    MAGGOT_CATERPILLAR_HUSK_TRIANGLE_COUNT      = 12,
    MAGGOT_CATERPILLAR_HUSK_QUAD_COUNT          = 32
};

static TmdBone _gMaggotCaterpillarHuskModelSkeleton[] = {
#include "assets/caterpillar_maggot_burst_head_skeleton.inc"
};

static u32 _gMaggotCaterpillarHuskModelPartVerts[] = {
#include "assets/caterpillar_maggot_burst_head_partVerts.inc"
};

static SVECTOR _gMaggotCaterpillarHuskModelVerts[] = {
#include "assets/caterpillar_maggot_burst_head_verts.inc"
};

static SVECTOR _gMaggotCaterpillarHuskModelNormals[] = {
#include "assets/caterpillar_maggot_burst_head_normals.inc"
};

static u32 _gMaggotCaterpillarHuskModelStream[] = {
#include "assets/caterpillar_maggot_burst_head_stream.inc"
};

/// One-part husk flung from body coordinate 4 on the enemy's burst death.
///
/// Each carrier owns a separate copy. Effect objects borrow this source and
/// its arrays until teardown, so the carrier must remain loaded. Model creation
/// resolves handler slots in the writable stream and updates `handlersResolved`.
/// The initial pose is one identity root; 40 vertices and 40 normals supply
/// 12 textured triangles and 32 textured quads, with no pre-transformed region.
/// Each primitive-buffer half holds those packets; placement texture-page and
/// CLUT offsets belong to the effect object's buffers rather than this source.
static TmdSource _gMaggotCaterpillarHuskModel = {
    .handlersResolved    = MAGGOT_CATERPILLAR_HUSK_HANDLERS_UNRESOLVED,
    .bufferHalfBytes     = MAGGOT_CATERPILLAR_HUSK_TRIANGLE_COUNT * sizeof(POLY_GT3) + MAGGOT_CATERPILLAR_HUSK_QUAD_COUNT * sizeof(POLY_GT4),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gMaggotCaterpillarHuskModelSkeleton),
    .partVertexCounts    = _gMaggotCaterpillarHuskModelPartVerts,
    .verts               = _gMaggotCaterpillarHuskModelVerts,
    .normals             = _gMaggotCaterpillarHuskModelNormals,
    .skeleton            = _gMaggotCaterpillarHuskModelSkeleton,
    .stream              = _gMaggotCaterpillarHuskModelStream,
};
