#include "mappic/mappic.h"

#include <psyq/libgpu.h>

#include "common.h"

/// Identity transform and self-parented root for the Helicopter landing pad shape.
static TmdBone _gMappicS102HelicopterLandingPadShapeSkeleton[] = {
#include "assets/mappic_s2_02_model_00C48_00070_skeleton.inc"
};

/// Vertex count of the Helicopter landing pad shape's sole part (four vertices).
static u32 _gMappicS102HelicopterLandingPadShapePartVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00070_partVerts.inc"
};

/// Helicopter landing pad's map footprint in integer model X/Z coordinates, with Y zero.
static SVECTOR _gMappicS102HelicopterLandingPadShapeVerts[] = {
#include "assets/mappic_s2_02_model_00C48_00070_verts.inc"
};

/// One unlit quad's packed vertex byte offsets and white colour, then two group ends and the stream end.
static u32 _gMappicS102HelicopterLandingPadShapeStream[] = {
#include "assets/mappic_s2_02_model_00C48_00070_stream.inc"
};

TmdSource gMappicS102HelicopterLandingPadShape = {
    .bufferHalfBytes  = sizeof(POLY_F4),
    .partCount        = ARRAY_SIZE(_gMappicS102HelicopterLandingPadShapeSkeleton),
    .partVertexCounts = _gMappicS102HelicopterLandingPadShapePartVerts,
    .verts            = _gMappicS102HelicopterLandingPadShapeVerts,
    .normals          = &_gMappicS102HelicopterLandingPadShapeVerts[ARRAY_SIZE(_gMappicS102HelicopterLandingPadShapeVerts)], // Empty normal pool at the vertex end.
    .skeleton         = _gMappicS102HelicopterLandingPadShapeSkeleton,
    .stream           = _gMappicS102HelicopterLandingPadShapeStream,
};
