#include "aya/aya.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/collision.h"

/// Packed facial-frame dimensions, in 16-bit VRAM words and rows.
enum {
    AYA_10200_EYES_FRAME_WIDTH_WORDS  = 25,
    AYA_10200_EYES_FRAME_HEIGHT_ROWS  = 16,
    AYA_10200_MOUTH_FRAME_WIDTH_WORDS = 14,
    AYA_10200_MOUTH_FRAME_HEIGHT_ROWS = 20,
};

/// Initial state of this package's writable model command stream.
enum { AYA_10200_BODY_MODEL_HANDLERS_UNRESOLVED = 0 };

/// Initial local transform and self-parented root for first-pair hand slot 0.
///
/// Read-only source tables retain their serialized positions in the data section.
static const TmdBone _gAya10200FirstPairHand0ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_10200_model_0019C_skeleton.inc"
};

/// Vertex count of the hand's sole part: 23.
static const u32 _gAya10200FirstPairHand0ModelPartVerts[ARRAY_SIZE(_gAya10200FirstPairHand0ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_10200_model_0019C_partVerts.inc"
};

/// Writable first-pair hand-0 vertices in integer coordinates local to the wrist.
static SVECTOR _gAya10200FirstPairHand0ModelVerts[23] = {
#include "assets/aya_10200_model_0019C_verts.inc"
};

/// Writable first-pair hand-0 lighting normals with twelve fractional bits and independent indices.
static SVECTOR _gAya10200FirstPairHand0ModelNormals[23] = {
#include "assets/aya_10200_model_0019C_normals.inc"
};

/// Writable commands for two textured triangles and twenty textured quads, ending in `TMD_STREAM_END`.
///
/// First model creation resolves each command's draw-handler slot in place.
static u32 _gAya10200FirstPairHand0ModelStream[161] = {
#include "assets/aya_10200_model_0019C_stream.inc"
};

TmdSource gAya10200FirstPairHand0Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (20 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAya10200FirstPairHand0ModelSkeleton),
    .partVertexCounts    = _gAya10200FirstPairHand0ModelPartVerts,
    .verts               = _gAya10200FirstPairHand0ModelVerts,
    .normals             = _gAya10200FirstPairHand0ModelNormals,
    .skeleton            = _gAya10200FirstPairHand0ModelSkeleton,
    .stream              = _gAya10200FirstPairHand0ModelStream,
};

/// Initial local transform and self-parented root for second-pair hand slot 0.
///
/// Read-only source tables retain their serialized positions in the data section.
static const TmdBone _gAya10200SecondPairHand0ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_10200_model_0019C_skeleton.inc"
};

/// Vertex count of the hand's sole part: 23.
static const u32 _gAya10200SecondPairHand0ModelPartVerts[ARRAY_SIZE(_gAya10200SecondPairHand0ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_10200_model_0019C_partVerts.inc"
};

/// Writable second-pair hand-0 vertices in integer coordinates local to the wrist.
static SVECTOR _gAya10200SecondPairHand0ModelVerts[23] = {
#include "assets/aya_10200_model_0019C_verts.inc"
};

/// Writable second-pair hand-0 lighting normals with twelve fractional bits and independent indices.
static SVECTOR _gAya10200SecondPairHand0ModelNormals[23] = {
#include "assets/aya_10200_model_0019C_normals.inc"
};

/// Writable commands for two textured triangles and twenty textured quads, ending in `TMD_STREAM_END`.
///
/// First model creation resolves each command's draw-handler slot in place.
static u32 _gAya10200SecondPairHand0ModelStream[161] = {
#include "assets/aya_10200_model_0019C_stream.inc"
};

TmdSource gAya10200SecondPairHand0Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (20 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAya10200SecondPairHand0ModelSkeleton),
    .partVertexCounts    = _gAya10200SecondPairHand0ModelPartVerts,
    .verts               = _gAya10200SecondPairHand0ModelVerts,
    .normals             = _gAya10200SecondPairHand0ModelNormals,
    .skeleton            = _gAya10200SecondPairHand0ModelSkeleton,
    .stream              = _gAya10200SecondPairHand0ModelStream,
};

static TmdBone _gAya10200Model00A5CSkeleton[1] = {
#include "assets/aya_10200_model_00A5C_skeleton.inc"
};

static u32 _gAya10200Model00A5CPartVerts[1] = {
#include "assets/aya_10200_model_00A5C_partVerts.inc"
};

static SVECTOR _gAya10200Model00A5CVerts[27] = {
#include "assets/aya_10200_model_00A5C_verts.inc"
};

static SVECTOR _gAya10200Model00A5CNormals[27] = {
#include "assets/aya_10200_model_00A5C_normals.inc"
};

static u32 _gAya10200Model00A5CStream[189] = {
#include "assets/aya_10200_model_00A5C_stream.inc"
};

TmdSource D_aya_10200_801164C0 = {
    0,
    1328,
    0,
    1,
    _gAya10200Model00A5CPartVerts,
    _gAya10200Model00A5CVerts,
    _gAya10200Model00A5CNormals,
    _gAya10200Model00A5CSkeleton,
    _gAya10200Model00A5CStream,
};

static TmdBone _gAya10200Model00F0CSkeleton[1] = {
#include "assets/aya_10200_model_00F0C_skeleton.inc"
};

static u32 _gAya10200Model00F0CPartVerts[1] = {
#include "assets/aya_10200_model_00F0C_partVerts.inc"
};

static SVECTOR _gAya10200Model00F0CVerts[23] = {
#include "assets/aya_10200_model_00F0C_verts.inc"
};

static SVECTOR _gAya10200Model00F0CNormals[23] = {
#include "assets/aya_10200_model_00F0C_normals.inc"
};

static u32 _gAya10200Model00F0CStream[161] = {
#include "assets/aya_10200_model_00F0C_stream.inc"
};

TmdSource D_aya_10200_80116900 = {
    0,
    1120,
    0,
    1,
    _gAya10200Model00F0CPartVerts,
    _gAya10200Model00F0CVerts,
    _gAya10200Model00F0CNormals,
    _gAya10200Model00F0CSkeleton,
    _gAya10200Model00F0CStream,
};

/// Nineteen initial part transforms and parent indices for the player body.
///
/// Read-only tables retain their serialized positions in the package's data section.
static const TmdBone _gAya10200BodyModelSkeleton[19] __attribute__((section(".data"))) = {
#include "assets/aya_10200_model_02B58_skeleton.inc"
};

/// Part-local vertex counts in skeleton order, totaling 352 vertices.
static const u32 _gAya10200BodyModelPartVerts[ARRAY_SIZE(_gAya10200BodyModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_10200_model_02B58_partVerts.inc"
};

/// Writable player-body vertices in integer coordinates local to each part.
static SVECTOR _gAya10200BodyModelVerts[352] = {
#include "assets/aya_10200_model_02B58_verts.inc"
};

/// Writable part-local body normals, with 12 fractional bits and independent stream indices.
static SVECTOR _gAya10200BodyModelNormals[373] = {
#include "assets/aya_10200_model_02B58_normals.inc"
};

/// Body draw commands whose handler slots are resolved in place on first model creation.
static u32 _gAya10200BodyModelStream[4051] = {
#include "assets/aya_10200_model_02B58_stream.inc"
};

TmdSource gAya10200BodyModel = {
    AYA_10200_BODY_MODEL_HANDLERS_UNRESOLVED,
    22344,
    5824,
    ARRAY_SIZE(_gAya10200BodyModelSkeleton),
    _gAya10200BodyModelPartVerts,
    _gAya10200BodyModelVerts,
    _gAya10200BodyModelNormals,
    _gAya10200BodyModelSkeleton,
    _gAya10200BodyModelStream,
};

static u_long _gAya10200Image06AC4[] = {
#include "assets/aya_10200_image_06AC4.inc"
};
/// One open-eyes frame followed by the upload terminator; the destination is rewritten at playback.
static GpuImageUpload _gAya10200EyesOpenUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10200_EYES_FRAME_WIDTH_WORDS, AYA_10200_EYES_FRAME_HEIGHT_ROWS }, _gAya10200Image06AC4 },
    { GP_IMG_REC_END },
};

static u_long _gAya10200Image06E04[] = {
#include "assets/aya_10200_image_06E04.inc"
};
/// One half-closed-eyes frame followed by the upload terminator; the destination is rewritten at playback.
static GpuImageUpload _gAya10200EyesHalfClosedUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10200_EYES_FRAME_WIDTH_WORDS, AYA_10200_EYES_FRAME_HEIGHT_ROWS }, _gAya10200Image06E04 },
    { GP_IMG_REC_END },
};

static u_long _gAya10200Image07144[] = {
#include "assets/aya_10200_image_07144.inc"
};
/// One closed-eyes frame followed by the upload terminator; the destination is rewritten at playback.
static GpuImageUpload _gAya10200EyesClosedUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10200_EYES_FRAME_WIDTH_WORDS, AYA_10200_EYES_FRAME_HEIGHT_ROWS }, _gAya10200Image07144 },
    { GP_IMG_REC_END },
};

/// Default mouth frame: packed 8-bit indices, two texels per VRAM word and four per u_long.
static u_long _gAya10200MouthRestPixels[AYA_10200_MOUTH_FRAME_WIDTH_WORDS * AYA_10200_MOUTH_FRAME_HEIGHT_ROWS / 2] = {
#include "assets/aya_10200_image_07484.inc"
};
/// One default mouth frame followed by the upload terminator; the destination is rewritten at playback.
static GpuImageUpload _gAya10200MouthRestUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10200_MOUTH_FRAME_WIDTH_WORDS, AYA_10200_MOUTH_FRAME_HEIGHT_ROWS }, _gAya10200MouthRestPixels },
    { GP_IMG_REC_END },
};

/// Alternate mouth selector's packed 8-bit frame, identical to the default frame in this package.
static u_long _gAya10200MouthAlternatePixels[AYA_10200_MOUTH_FRAME_WIDTH_WORDS * AYA_10200_MOUTH_FRAME_HEIGHT_ROWS / 2] = {
#include "assets/aya_10200_image_07484.inc"
};
/// One alternate mouth frame followed by the upload terminator; the destination is rewritten at playback.
static GpuImageUpload _gAya10200MouthAlternateUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10200_MOUTH_FRAME_WIDTH_WORDS, AYA_10200_MOUTH_FRAME_HEIGHT_ROWS }, _gAya10200MouthAlternatePixels },
    { GP_IMG_REC_END },
};

GpuImageUpload* gAya10200EyesRestSequence[]    = { _gAya10200EyesOpenUploads, NULL };
GpuImageUpload* gAya10200EyesClosingSequence[] = { _gAya10200EyesOpenUploads, _gAya10200EyesHalfClosedUploads, _gAya10200EyesClosedUploads, NULL };
GpuImageUpload* gAya10200EyesOpeningSequence[] = { _gAya10200EyesClosedUploads, _gAya10200EyesHalfClosedUploads, _gAya10200EyesOpenUploads, NULL };
GpuImageUpload* gAya10200EyesBlinkSequence[]   = {
    _gAya10200EyesOpenUploads,
    _gAya10200EyesHalfClosedUploads,
    _gAya10200EyesClosedUploads,
    _gAya10200EyesHalfClosedUploads,
    _gAya10200EyesOpenUploads,
    NULL,
};
GpuImageUpload* gAya10200MouthRestSequence[]      = { _gAya10200MouthRestUploads, NULL };
GpuImageUpload* gAya10200MouthAlternateSequence[] = { _gAya10200MouthAlternateUploads, NULL };
