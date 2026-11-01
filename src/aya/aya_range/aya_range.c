#include "aya/aya.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/gpu_image_upload.h"

/// Facial-frame dimensions, in 16-bit VRAM words and rows.
enum {
    AYA_RANGE_EYES_FRAME_WIDTH_WORDS  = 25,
    AYA_RANGE_EYES_FRAME_HEIGHT_ROWS  = 16,
    AYA_RANGE_MOUTH_FRAME_WIDTH_WORDS = 14,
    AYA_RANGE_MOUTH_FRAME_HEIGHT_ROWS = 20,
};

/// Initial local transform and self-parented root for first-pair hand slot 0.
///
/// Read-only source tables retain their serialized positions in the data section.
static const TmdBone _gAyaRangeFirstPairHand0ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_0019C_skeleton.inc"
};

/// Vertex count of the hand's sole part: 23.
static const u32 _gAyaRangeFirstPairHand0ModelPartVerts[ARRAY_SIZE(_gAyaRangeFirstPairHand0ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_0019C_partVerts.inc"
};

/// Writable first-pair hand-0 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaRangeFirstPairHand0ModelVerts[23] = {
#include "assets/aya_mist_model_0019C_verts.inc"
};

/// Writable first-pair hand-0 lighting normals with twelve fractional bits and independent indices.
static SVECTOR _gAyaRangeFirstPairHand0ModelNormals[23] = {
#include "assets/aya_mist_model_0019C_normals.inc"
};

/// Writable commands for two textured triangles and twenty textured quads, ending in `TMD_STREAM_END`.
///
/// First model creation resolves each command's draw-handler slot in place.
static u32 _gAyaRangeFirstPairHand0ModelStream[161] = {
#include "assets/aya_mist_model_0019C_stream.inc"
};

TmdSource gAyaRangeFirstPairHand0Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (20 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaRangeFirstPairHand0ModelSkeleton),
    .partVertexCounts    = _gAyaRangeFirstPairHand0ModelPartVerts,
    .verts               = _gAyaRangeFirstPairHand0ModelVerts,
    .normals             = _gAyaRangeFirstPairHand0ModelNormals,
    .skeleton            = _gAyaRangeFirstPairHand0ModelSkeleton,
    .stream              = _gAyaRangeFirstPairHand0ModelStream,
};

/// Initial local transform and self-parented root for second-pair hand slot 0.
///
/// Read-only source tables retain their serialized positions in the data section.
static const TmdBone _gAyaRangeSecondPairHand0ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_0019C_skeleton.inc"
};

/// Vertex count of the hand's sole part: 23.
static const u32 _gAyaRangeSecondPairHand0ModelPartVerts[ARRAY_SIZE(_gAyaRangeSecondPairHand0ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_0019C_partVerts.inc"
};

/// Writable second-pair hand-0 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaRangeSecondPairHand0ModelVerts[23] = {
#include "assets/aya_mist_model_0019C_verts.inc"
};

/// Writable second-pair hand-0 lighting normals with twelve fractional bits and independent indices.
static SVECTOR _gAyaRangeSecondPairHand0ModelNormals[23] = {
#include "assets/aya_mist_model_0019C_normals.inc"
};

/// Writable commands for two textured triangles and twenty textured quads, ending in `TMD_STREAM_END`.
///
/// First model creation resolves each command's draw-handler slot in place.
static u32 _gAyaRangeSecondPairHand0ModelStream[161] = {
#include "assets/aya_mist_model_0019C_stream.inc"
};

TmdSource gAyaRangeSecondPairHand0Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (20 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaRangeSecondPairHand0ModelSkeleton),
    .partVertexCounts    = _gAyaRangeSecondPairHand0ModelPartVerts,
    .verts               = _gAyaRangeSecondPairHand0ModelVerts,
    .normals             = _gAyaRangeSecondPairHand0ModelNormals,
    .skeleton            = _gAyaRangeSecondPairHand0ModelSkeleton,
    .stream              = _gAyaRangeSecondPairHand0ModelStream,
};

/// Initial local transform and self-parented root for first-pair hand slot 1.
///
/// Read-only source tables retain their serialized positions in the data section.
static const TmdBone _gAyaRangeFirstPairHand1ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_00A5C_skeleton.inc"
};

/// Vertex count of the hand's sole part: 27.
static const u32 _gAyaRangeFirstPairHand1ModelPartVerts[ARRAY_SIZE(_gAyaRangeFirstPairHand1ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_00A5C_partVerts.inc"
};

/// Writable first-pair hand-1 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaRangeFirstPairHand1ModelVerts[27] = {
#include "assets/aya_mist_model_00A5C_verts.inc"
};

/// Writable part-local lighting normals with twelve fractional bits and independent indices.
static SVECTOR _gAyaRangeFirstPairHand1ModelNormals[27] = {
#include "assets/aya_mist_model_00A5C_normals.inc"
};

/// Writable commands for two textured triangles and 24 textured quads, ending in `TMD_STREAM_END`.
///
/// First model creation resolves each command's draw-handler slot in place.
static u32 _gAyaRangeFirstPairHand1ModelStream[189] = {
#include "assets/aya_mist_model_00A5C_stream.inc"
};

TmdSource gAyaRangeFirstPairHand1Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaRangeFirstPairHand1ModelSkeleton),
    .partVertexCounts    = _gAyaRangeFirstPairHand1ModelPartVerts,
    .verts               = _gAyaRangeFirstPairHand1ModelVerts,
    .normals             = _gAyaRangeFirstPairHand1ModelNormals,
    .skeleton            = _gAyaRangeFirstPairHand1ModelSkeleton,
    .stream              = _gAyaRangeFirstPairHand1ModelStream,
};

/// Initial local transform and self-parented root for second-pair hand slot 1.
///
/// Read-only source tables retain their serialized positions in the data section.
static const TmdBone _gAyaRangeSecondPairHand1ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_00F0C_skeleton.inc"
};

/// Vertex count of the hand's sole part: 23.
static const u32 _gAyaRangeSecondPairHand1ModelPartVerts[ARRAY_SIZE(_gAyaRangeSecondPairHand1ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_00F0C_partVerts.inc"
};

/// Writable second-pair hand-1 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaRangeSecondPairHand1ModelVerts[23] = {
#include "assets/aya_mist_model_00F0C_verts.inc"
};

/// Writable part-local lighting normals with twelve fractional bits and independent indices.
static SVECTOR _gAyaRangeSecondPairHand1ModelNormals[23] = {
#include "assets/aya_mist_model_00F0C_normals.inc"
};

/// Writable commands for two textured triangles and twenty textured quads, ending in `TMD_STREAM_END`.
///
/// First model creation resolves each command's draw-handler slot in place.
static u32 _gAyaRangeSecondPairHand1ModelStream[161] = {
#include "assets/aya_mist_model_00F0C_stream.inc"
};

TmdSource gAyaRangeSecondPairHand1Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (20 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaRangeSecondPairHand1ModelSkeleton),
    .partVertexCounts    = _gAyaRangeSecondPairHand1ModelPartVerts,
    .verts               = _gAyaRangeSecondPairHand1ModelVerts,
    .normals             = _gAyaRangeSecondPairHand1ModelNormals,
    .skeleton            = _gAyaRangeSecondPairHand1ModelSkeleton,
    .stream              = _gAyaRangeSecondPairHand1ModelStream,
};

/// Initial body-part transforms and hierarchy, with part 0 as the self-parented root.
///
/// Read-only source tables retain their serialized positions in the data section.
static const TmdBone _gAyaRangeBodyModelSkeleton[19] __attribute__((section(".data"))) = {
#include "assets/aya_range_model_02BB8_skeleton.inc"
};

/// Part-local vertex counts in skeleton order, totaling 358 vertices.
static const u32 _gAyaRangeBodyModelPartVerts[ARRAY_SIZE(_gAyaRangeBodyModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_range_model_02BB8_partVerts.inc"
};

/// Writable body vertices in integer coordinates local to each model part.
static SVECTOR _gAyaRangeBodyModelVerts[358] = {
#include "assets/aya_range_model_02BB8_verts.inc"
};

/// Writable part-local lighting normals with twelve fractional bits and independent stream indices.
static SVECTOR _gAyaRangeBodyModelNormals[379] = {
#include "assets/aya_range_model_02BB8_normals.inc"
};

/// Writable commands for 290 textured triangles and 211 textured quads, ending in `TMD_STREAM_END`.
///
/// Handler resolution rewrites the command slots on first model creation.
/// The final group draws 82 triangles and 50 quads from the transformed cache.
static u32 _gAyaRangeBodyModelStream[4080] = {
#include "assets/aya_range_model_02BB8_stream.inc"
};

TmdSource gAyaRangeBodyModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (290 * sizeof(POLY_GT3)) + (211 * sizeof(POLY_GT4)),
    .preXformRegionBytes = (82 * sizeof(POLY_GT3)) + (50 * sizeof(POLY_GT4)),
    .partCount           = ARRAY_SIZE(_gAyaRangeBodyModelSkeleton),
    .partVertexCounts    = _gAyaRangeBodyModelPartVerts,
    .verts               = _gAyaRangeBodyModelVerts,
    .normals             = _gAyaRangeBodyModelNormals,
    .skeleton            = _gAyaRangeBodyModelSkeleton,
    .stream              = _gAyaRangeBodyModelStream,
};

static u_long _gAyaRangeEyesOpenPixels[] = {
#include "assets/aya_mist_image_06AC4.inc"
};
/// One open-eyes upload and its terminator; playback supplies the texture-relative destination.
///
/// Records remain writable, and their packed 8-bit pixels stay loaded until
/// the queued GPU transfer completes.
static GpuImageUpload _gAyaRangeEyesOpenUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_RANGE_EYES_FRAME_WIDTH_WORDS, AYA_RANGE_EYES_FRAME_HEIGHT_ROWS }, _gAyaRangeEyesOpenPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long _gAyaRangeEyesHalfClosedPixels[] = {
#include "assets/aya_mist_image_06E04.inc"
};
/// One half-closed-eyes upload and its terminator, with writable destination and borrowed pixels.
static GpuImageUpload _gAyaRangeEyesHalfClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_RANGE_EYES_FRAME_WIDTH_WORDS, AYA_RANGE_EYES_FRAME_HEIGHT_ROWS }, _gAyaRangeEyesHalfClosedPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long _gAyaRangeEyesClosedPixels[] = {
#include "assets/aya_mist_image_07144.inc"
};
/// One closed-eyes upload and its terminator, with writable destination and borrowed pixels.
static GpuImageUpload _gAyaRangeEyesClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_RANGE_EYES_FRAME_WIDTH_WORDS, AYA_RANGE_EYES_FRAME_HEIGHT_ROWS }, _gAyaRangeEyesClosedPixels },
    { GPU_IMAGE_UPLOAD_END },
};

/// Resting mouth pixels: packed 8-bit palette indices, four texels per u_long.
static u_long _gAyaRangeMouthRestPixels[AYA_RANGE_MOUTH_FRAME_WIDTH_WORDS * AYA_RANGE_MOUTH_FRAME_HEIGHT_ROWS / 2] = {
#include "assets/aya_mist_image_07484.inc"
};
/// One resting-mouth upload and its terminator, with writable destination and borrowed pixels.
static GpuImageUpload _gAyaRangeMouthRestUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_RANGE_MOUTH_FRAME_WIDTH_WORDS, AYA_RANGE_MOUTH_FRAME_HEIGHT_ROWS }, _gAyaRangeMouthRestPixels },
    { GPU_IMAGE_UPLOAD_END },
};

/// Alternate mouth selector's packed 8-bit pixels, identical here to the resting mouth.
static u_long _gAyaRangeMouthAlternatePixels[AYA_RANGE_MOUTH_FRAME_WIDTH_WORDS * AYA_RANGE_MOUTH_FRAME_HEIGHT_ROWS / 2] = {
#include "assets/aya_mist_image_07484.inc"
};
/// One alternate-mouth upload and its terminator, with writable destination and borrowed pixels.
static GpuImageUpload _gAyaRangeMouthAlternateUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_RANGE_MOUTH_FRAME_WIDTH_WORDS, AYA_RANGE_MOUTH_FRAME_HEIGHT_ROWS }, _gAyaRangeMouthAlternatePixels },
    { GPU_IMAGE_UPLOAD_END },
};

GpuImageUpload* const gAyaRangeEyesRestSequence[2] __attribute__((section(".data")))    = { _gAyaRangeEyesOpenUploads, NULL };
GpuImageUpload* const gAyaRangeEyesClosingSequence[4] __attribute__((section(".data"))) = { _gAyaRangeEyesOpenUploads, _gAyaRangeEyesHalfClosedUploads, _gAyaRangeEyesClosedUploads, NULL };
GpuImageUpload* const gAyaRangeEyesOpeningSequence[4] __attribute__((section(".data"))) = { _gAyaRangeEyesClosedUploads, _gAyaRangeEyesHalfClosedUploads, _gAyaRangeEyesOpenUploads, NULL };
GpuImageUpload* const gAyaRangeEyesBlinkSequence[6] __attribute__((section(".data")))   = {
    _gAyaRangeEyesOpenUploads,
    _gAyaRangeEyesHalfClosedUploads,
    _gAyaRangeEyesClosedUploads,
    _gAyaRangeEyesHalfClosedUploads,
    _gAyaRangeEyesOpenUploads,
    NULL,
};
GpuImageUpload* const gAyaRangeMouthRestSequence[2] __attribute__((section(".data")))      = { _gAyaRangeMouthRestUploads, NULL };
GpuImageUpload* const gAyaRangeMouthAlternateSequence[2] __attribute__((section(".data"))) = { _gAyaRangeMouthAlternateUploads, NULL };
