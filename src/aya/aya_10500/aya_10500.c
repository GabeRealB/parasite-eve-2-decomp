#include "aya/aya.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/collision.h"

/// Facial-frame dimensions, in 16-bit VRAM words and rows.
enum {
    AYA_10500_EYES_FRAME_WIDTH_WORDS  = 25,
    AYA_10500_EYES_FRAME_HEIGHT_ROWS  = 16,
    AYA_10500_MOUTH_FRAME_WIDTH_WORDS = 14,
    AYA_10500_MOUTH_FRAME_HEIGHT_ROWS = 20,
};

static TmdBone _gAya10500Aya10200Model0019CSkeleton[1] = {
#include "assets/aya_10200_model_0019C_skeleton.inc"
};

static u32 _gAya10500Aya10200Model0019CPartVerts[1] = {
#include "assets/aya_10200_model_0019C_partVerts.inc"
};

static SVECTOR _gAya10500Aya10200Model0019CVerts[23] = {
#include "assets/aya_10200_model_0019C_verts.inc"
};

static SVECTOR _gAya10500Aya10200Model0019CNormals[23] = {
#include "assets/aya_10200_model_0019C_normals.inc"
};

static u32 _gAya10500Aya10200Model0019CStream[161] = {
#include "assets/aya_10200_model_0019C_stream.inc"
};

TmdSource D_aya_10500_80115B90 = {
    0,
    1120,
    0,
    1,
    _gAya10500Aya10200Model0019CPartVerts,
    _gAya10500Aya10200Model0019CVerts,
    _gAya10500Aya10200Model0019CNormals,
    _gAya10500Aya10200Model0019CSkeleton,
    _gAya10500Aya10200Model0019CStream,
};

static TmdBone _gAya10500Aya10200Model0019CAt00860Skeleton[1] = {
#include "assets/aya_10200_model_0019C_skeleton.inc"
};

static u32 _gAya10500Aya10200Model0019CAt00860PartVerts[1] = {
#include "assets/aya_10200_model_0019C_partVerts.inc"
};

static SVECTOR _gAya10500Aya10200Model0019CAt00860Verts[23] = {
#include "assets/aya_10200_model_0019C_verts.inc"
};

static SVECTOR _gAya10500Aya10200Model0019CAt00860Normals[23] = {
#include "assets/aya_10200_model_0019C_normals.inc"
};

static u32 _gAya10500Aya10200Model0019CAt00860Stream[161] = {
#include "assets/aya_10200_model_0019C_stream.inc"
};

TmdSource D_aya_10500_80115FD0 = {
    0,
    1120,
    0,
    1,
    _gAya10500Aya10200Model0019CAt00860PartVerts,
    _gAya10500Aya10200Model0019CAt00860Verts,
    _gAya10500Aya10200Model0019CAt00860Normals,
    _gAya10500Aya10200Model0019CAt00860Skeleton,
    _gAya10500Aya10200Model0019CAt00860Stream,
};

static TmdBone _gAya10500Aya10200Model00A5CSkeleton[1] = {
#include "assets/aya_10200_model_00A5C_skeleton.inc"
};

static u32 _gAya10500Aya10200Model00A5CPartVerts[1] = {
#include "assets/aya_10200_model_00A5C_partVerts.inc"
};

static SVECTOR _gAya10500Aya10200Model00A5CVerts[27] = {
#include "assets/aya_10200_model_00A5C_verts.inc"
};

static SVECTOR _gAya10500Aya10200Model00A5CNormals[27] = {
#include "assets/aya_10200_model_00A5C_normals.inc"
};

static u32 _gAya10500Aya10200Model00A5CStream[189] = {
#include "assets/aya_10200_model_00A5C_stream.inc"
};

TmdSource D_aya_10500_801164C0 = {
    0,
    1328,
    0,
    1,
    _gAya10500Aya10200Model00A5CPartVerts,
    _gAya10500Aya10200Model00A5CVerts,
    _gAya10500Aya10200Model00A5CNormals,
    _gAya10500Aya10200Model00A5CSkeleton,
    _gAya10500Aya10200Model00A5CStream,
};

static TmdBone _gAya10500Aya10200Model00F0CSkeleton[1] = {
#include "assets/aya_10200_model_00F0C_skeleton.inc"
};

static u32 _gAya10500Aya10200Model00F0CPartVerts[1] = {
#include "assets/aya_10200_model_00F0C_partVerts.inc"
};

static SVECTOR _gAya10500Aya10200Model00F0CVerts[23] = {
#include "assets/aya_10200_model_00F0C_verts.inc"
};

static SVECTOR _gAya10500Aya10200Model00F0CNormals[23] = {
#include "assets/aya_10200_model_00F0C_normals.inc"
};

static u32 _gAya10500Aya10200Model00F0CStream[161] = {
#include "assets/aya_10200_model_00F0C_stream.inc"
};

TmdSource D_aya_10500_80116900 = {
    0,
    1120,
    0,
    1,
    _gAya10500Aya10200Model00F0CPartVerts,
    _gAya10500Aya10200Model00F0CVerts,
    _gAya10500Aya10200Model00F0CNormals,
    _gAya10500Aya10200Model00F0CSkeleton,
    _gAya10500Aya10200Model00F0CStream,
};

/// Initial body-part transforms and hierarchy, with part 0 as the self-parented root.
///
/// Read-only source tables retain their serialized positions in the data section.
static const TmdBone _gAya10500BodyModelSkeleton[19] __attribute__((section(".data"))) = {
#include "assets/aya_10500_model_02BB8_skeleton.inc"
};

/// Part-local vertex counts in skeleton order, totaling 358 vertices.
static const u32 _gAya10500BodyModelPartVerts[ARRAY_SIZE(_gAya10500BodyModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_10500_model_02BB8_partVerts.inc"
};

/// Writable body vertices in integer coordinates local to each model part.
static SVECTOR _gAya10500BodyModelVerts[358] = {
#include "assets/aya_10500_model_02BB8_verts.inc"
};

/// Writable part-local lighting normals with twelve fractional bits and independent stream indices.
static SVECTOR _gAya10500BodyModelNormals[379] = {
#include "assets/aya_10500_model_02BB8_normals.inc"
};

/// Writable commands for 290 textured triangles and 211 textured quads, ending in `TMD_STREAM_END`.
///
/// Handler resolution rewrites the command slots on first model creation.
/// The final group draws 82 triangles and 50 quads from the transformed cache.
static u32 _gAya10500BodyModelStream[4080] = {
#include "assets/aya_10500_model_02BB8_stream.inc"
};

TmdSource gAya10500BodyModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (290 * sizeof(POLY_GT3)) + (211 * sizeof(POLY_GT4)),
    .preXformRegionBytes = (82 * sizeof(POLY_GT3)) + (50 * sizeof(POLY_GT4)),
    .partCount           = ARRAY_SIZE(_gAya10500BodyModelSkeleton),
    .partVertexCounts    = _gAya10500BodyModelPartVerts,
    .verts               = _gAya10500BodyModelVerts,
    .normals             = _gAya10500BodyModelNormals,
    .skeleton            = _gAya10500BodyModelSkeleton,
    .stream              = _gAya10500BodyModelStream,
};

static u_long _gAya10500Aya10200Image06AC4[] = {
#include "assets/aya_10200_image_06AC4.inc"
};
/// One open-eyes upload and its terminator; playback supplies the texture-relative destination.
///
/// Records remain writable, and their packed 8-bit pixels stay loaded until
/// the queued GPU transfer completes.
static GpuImageUpload _gAya10500EyesOpenUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10500_EYES_FRAME_WIDTH_WORDS, AYA_10500_EYES_FRAME_HEIGHT_ROWS }, _gAya10500Aya10200Image06AC4 },
    { GP_IMG_REC_END },
};

static u_long _gAya10500Aya10200Image06E04[] = {
#include "assets/aya_10200_image_06E04.inc"
};
/// One half-closed-eyes upload and its terminator, with writable destination and borrowed pixels.
static GpuImageUpload _gAya10500EyesHalfClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10500_EYES_FRAME_WIDTH_WORDS, AYA_10500_EYES_FRAME_HEIGHT_ROWS }, _gAya10500Aya10200Image06E04 },
    { GP_IMG_REC_END },
};

static u_long _gAya10500Aya10200Image07144[] = {
#include "assets/aya_10200_image_07144.inc"
};
/// One closed-eyes upload and its terminator, with writable destination and borrowed pixels.
static GpuImageUpload _gAya10500EyesClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10500_EYES_FRAME_WIDTH_WORDS, AYA_10500_EYES_FRAME_HEIGHT_ROWS }, _gAya10500Aya10200Image07144 },
    { GP_IMG_REC_END },
};

/// Resting mouth pixels: packed 8-bit palette indices, four texels per u_long.
static u_long _gAya10500MouthRestPixels[AYA_10500_MOUTH_FRAME_WIDTH_WORDS * AYA_10500_MOUTH_FRAME_HEIGHT_ROWS / 2] = {
#include "assets/aya_10200_image_07484.inc"
};
/// One resting-mouth upload and its terminator, with writable destination and borrowed pixels.
static GpuImageUpload _gAya10500MouthRestUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10500_MOUTH_FRAME_WIDTH_WORDS, AYA_10500_MOUTH_FRAME_HEIGHT_ROWS }, _gAya10500MouthRestPixels },
    { GP_IMG_REC_END },
};

/// Alternate mouth selector's packed 8-bit pixels, identical here to the resting mouth.
static u_long _gAya10500MouthAlternatePixels[AYA_10500_MOUTH_FRAME_WIDTH_WORDS * AYA_10500_MOUTH_FRAME_HEIGHT_ROWS / 2] = {
#include "assets/aya_10200_image_07484.inc"
};
/// One alternate-mouth upload and its terminator, with writable destination and borrowed pixels.
static GpuImageUpload _gAya10500MouthAlternateUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10500_MOUTH_FRAME_WIDTH_WORDS, AYA_10500_MOUTH_FRAME_HEIGHT_ROWS }, _gAya10500MouthAlternatePixels },
    { GP_IMG_REC_END },
};

GpuImageUpload* const gAya10500EyesRestSequence[2] __attribute__((section(".data")))    = { _gAya10500EyesOpenUploads, NULL };
GpuImageUpload* const gAya10500EyesClosingSequence[4] __attribute__((section(".data"))) = { _gAya10500EyesOpenUploads, _gAya10500EyesHalfClosedUploads, _gAya10500EyesClosedUploads, NULL };
GpuImageUpload* const gAya10500EyesOpeningSequence[4] __attribute__((section(".data"))) = { _gAya10500EyesClosedUploads, _gAya10500EyesHalfClosedUploads, _gAya10500EyesOpenUploads, NULL };
GpuImageUpload* const gAya10500EyesBlinkSequence[6] __attribute__((section(".data")))   = {
    _gAya10500EyesOpenUploads,
    _gAya10500EyesHalfClosedUploads,
    _gAya10500EyesClosedUploads,
    _gAya10500EyesHalfClosedUploads,
    _gAya10500EyesOpenUploads,
    NULL,
};
GpuImageUpload* const gAya10500MouthRestSequence[2] __attribute__((section(".data")))      = { _gAya10500MouthRestUploads, NULL };
GpuImageUpload* const gAya10500MouthAlternateSequence[2] __attribute__((section(".data"))) = { _gAya10500MouthAlternateUploads, NULL };
