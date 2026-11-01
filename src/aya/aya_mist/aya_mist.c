#include "aya/aya.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/gpu_image_upload.h"

/// Packed facial-frame dimensions, in 16-bit VRAM words and rows.
enum {
    AYA_MIST_EYES_FRAME_WIDTH_WORDS  = 25,
    AYA_MIST_EYES_FRAME_HEIGHT_ROWS  = 16,
    AYA_MIST_MOUTH_FRAME_WIDTH_WORDS = 14,
    AYA_MIST_MOUTH_FRAME_HEIGHT_ROWS = 20,
};

/// Initial local transform and self-parented root for first-pair hand slot 0.
///
/// Read-only source tables retain their serialized positions in the data section.
static const TmdBone _gAyaMistFirstPairHand0ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_0019C_skeleton.inc"
};

/// Vertex count of the hand's sole part: 23.
static const u32 _gAyaMistFirstPairHand0ModelPartVerts[ARRAY_SIZE(_gAyaMistFirstPairHand0ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_0019C_partVerts.inc"
};

/// Writable first-pair hand-0 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaMistFirstPairHand0ModelVerts[23] = {
#include "assets/aya_mist_model_0019C_verts.inc"
};

/// Writable first-pair hand-0 lighting normals with twelve fractional bits and independent indices.
static SVECTOR _gAyaMistFirstPairHand0ModelNormals[23] = {
#include "assets/aya_mist_model_0019C_normals.inc"
};

/// Writable commands for two textured triangles and twenty textured quads, ending in `TMD_STREAM_END`.
///
/// First model creation resolves each command's draw-handler slot in place.
static u32 _gAyaMistFirstPairHand0ModelStream[161] = {
#include "assets/aya_mist_model_0019C_stream.inc"
};

TmdSource gAyaMistFirstPairHand0Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (20 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaMistFirstPairHand0ModelSkeleton),
    .partVertexCounts    = _gAyaMistFirstPairHand0ModelPartVerts,
    .verts               = _gAyaMistFirstPairHand0ModelVerts,
    .normals             = _gAyaMistFirstPairHand0ModelNormals,
    .skeleton            = _gAyaMistFirstPairHand0ModelSkeleton,
    .stream              = _gAyaMistFirstPairHand0ModelStream,
};

/// Initial local transform and self-parented root for second-pair hand slot 0.
///
/// Read-only source tables retain their serialized positions in the data section.
static const TmdBone _gAyaMistSecondPairHand0ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_0019C_skeleton.inc"
};

/// Vertex count of the hand's sole part: 23.
static const u32 _gAyaMistSecondPairHand0ModelPartVerts[ARRAY_SIZE(_gAyaMistSecondPairHand0ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_0019C_partVerts.inc"
};

/// Writable second-pair hand-0 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaMistSecondPairHand0ModelVerts[23] = {
#include "assets/aya_mist_model_0019C_verts.inc"
};

/// Writable second-pair hand-0 lighting normals with twelve fractional bits and independent indices.
static SVECTOR _gAyaMistSecondPairHand0ModelNormals[23] = {
#include "assets/aya_mist_model_0019C_normals.inc"
};

/// Writable commands for two textured triangles and twenty textured quads, ending in `TMD_STREAM_END`.
///
/// First model creation resolves each command's draw-handler slot in place.
static u32 _gAyaMistSecondPairHand0ModelStream[161] = {
#include "assets/aya_mist_model_0019C_stream.inc"
};

TmdSource gAyaMistSecondPairHand0Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (20 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaMistSecondPairHand0ModelSkeleton),
    .partVertexCounts    = _gAyaMistSecondPairHand0ModelPartVerts,
    .verts               = _gAyaMistSecondPairHand0ModelVerts,
    .normals             = _gAyaMistSecondPairHand0ModelNormals,
    .skeleton            = _gAyaMistSecondPairHand0ModelSkeleton,
    .stream              = _gAyaMistSecondPairHand0ModelStream,
};

/// Initial local transform and self-parented root for first-pair hand slot 1.
///
/// Read-only source tables retain their serialized positions in the data section.
static const TmdBone _gAyaMistFirstPairHand1ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_00A5C_skeleton.inc"
};

/// Vertex count of the hand's sole part: 27.
static const u32 _gAyaMistFirstPairHand1ModelPartVerts[ARRAY_SIZE(_gAyaMistFirstPairHand1ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_00A5C_partVerts.inc"
};

/// Writable first-pair hand-1 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaMistFirstPairHand1ModelVerts[27] = {
#include "assets/aya_mist_model_00A5C_verts.inc"
};

/// Writable part-local lighting normals with twelve fractional bits and independent indices.
static SVECTOR _gAyaMistFirstPairHand1ModelNormals[27] = {
#include "assets/aya_mist_model_00A5C_normals.inc"
};

/// Writable commands for two textured triangles and 24 textured quads, ending in `TMD_STREAM_END`.
///
/// First model creation resolves each command's draw-handler slot in place.
static u32 _gAyaMistFirstPairHand1ModelStream[189] = {
#include "assets/aya_mist_model_00A5C_stream.inc"
};

TmdSource gAyaMistFirstPairHand1Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaMistFirstPairHand1ModelSkeleton),
    .partVertexCounts    = _gAyaMistFirstPairHand1ModelPartVerts,
    .verts               = _gAyaMistFirstPairHand1ModelVerts,
    .normals             = _gAyaMistFirstPairHand1ModelNormals,
    .skeleton            = _gAyaMistFirstPairHand1ModelSkeleton,
    .stream              = _gAyaMistFirstPairHand1ModelStream,
};

/// Initial local transform and self-parented root for second-pair hand slot 1.
///
/// Read-only source tables retain their serialized positions in the data section.
static const TmdBone _gAyaMistSecondPairHand1ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_00F0C_skeleton.inc"
};

/// Vertex count of the hand's sole part: 23.
static const u32 _gAyaMistSecondPairHand1ModelPartVerts[ARRAY_SIZE(_gAyaMistSecondPairHand1ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_00F0C_partVerts.inc"
};

/// Writable second-pair hand-1 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaMistSecondPairHand1ModelVerts[23] = {
#include "assets/aya_mist_model_00F0C_verts.inc"
};

/// Writable part-local lighting normals with twelve fractional bits and independent indices.
static SVECTOR _gAyaMistSecondPairHand1ModelNormals[23] = {
#include "assets/aya_mist_model_00F0C_normals.inc"
};

/// Writable commands for two textured triangles and twenty textured quads, ending in `TMD_STREAM_END`.
///
/// First model creation resolves each command's draw-handler slot in place.
static u32 _gAyaMistSecondPairHand1ModelStream[161] = {
#include "assets/aya_mist_model_00F0C_stream.inc"
};

TmdSource gAyaMistSecondPairHand1Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (20 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaMistSecondPairHand1ModelSkeleton),
    .partVertexCounts    = _gAyaMistSecondPairHand1ModelPartVerts,
    .verts               = _gAyaMistSecondPairHand1ModelVerts,
    .normals             = _gAyaMistSecondPairHand1ModelNormals,
    .skeleton            = _gAyaMistSecondPairHand1ModelSkeleton,
    .stream              = _gAyaMistSecondPairHand1ModelStream,
};

/// The player body for resource variant 3, `gAyaMistBodyModel`, with its arrays: generated from the asset
/// (characters/aya/mist/aya_mist_body.usda, textured through characters/aya/mist/binding) by
/// tools/buildsteps/gen_model_source.py.
#include "assets/characters/aya/mist/body.inc"

static u_long _gAyaMistEyesOpenPixels[] = {
#include "assets/aya_mist_image_06AC4.inc"
};
/// One open-eyes frame followed by the upload terminator; the destination is rewritten at playback.
static GpuImageUpload _gAyaMistEyesOpenUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_MIST_EYES_FRAME_WIDTH_WORDS, AYA_MIST_EYES_FRAME_HEIGHT_ROWS }, _gAyaMistEyesOpenPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long _gAyaMistEyesHalfClosedPixels[] = {
#include "assets/aya_mist_image_06E04.inc"
};
/// One half-closed-eyes frame followed by the upload terminator; the destination is rewritten at playback.
static GpuImageUpload _gAyaMistEyesHalfClosedUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_MIST_EYES_FRAME_WIDTH_WORDS, AYA_MIST_EYES_FRAME_HEIGHT_ROWS }, _gAyaMistEyesHalfClosedPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long _gAyaMistEyesClosedPixels[] = {
#include "assets/aya_mist_image_07144.inc"
};
/// One closed-eyes frame followed by the upload terminator; the destination is rewritten at playback.
static GpuImageUpload _gAyaMistEyesClosedUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_MIST_EYES_FRAME_WIDTH_WORDS, AYA_MIST_EYES_FRAME_HEIGHT_ROWS }, _gAyaMistEyesClosedPixels },
    { GPU_IMAGE_UPLOAD_END },
};

/// Default mouth frame: packed 8-bit indices, two texels per VRAM word and four per u_long.
static u_long _gAyaMistMouthRestPixels[AYA_MIST_MOUTH_FRAME_WIDTH_WORDS * AYA_MIST_MOUTH_FRAME_HEIGHT_ROWS / 2] = {
#include "assets/aya_mist_image_07484.inc"
};
/// One default mouth frame followed by the upload terminator; the destination is rewritten at playback.
static GpuImageUpload _gAyaMistMouthRestUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_MIST_MOUTH_FRAME_WIDTH_WORDS, AYA_MIST_MOUTH_FRAME_HEIGHT_ROWS }, _gAyaMistMouthRestPixels },
    { GPU_IMAGE_UPLOAD_END },
};

/// Alternate mouth selector's packed 8-bit frame, identical to the default frame in this package.
static u_long _gAyaMistMouthAlternatePixels[AYA_MIST_MOUTH_FRAME_WIDTH_WORDS * AYA_MIST_MOUTH_FRAME_HEIGHT_ROWS / 2] = {
#include "assets/aya_mist_image_07484.inc"
};
/// One alternate mouth frame followed by the upload terminator; the destination is rewritten at playback.
static GpuImageUpload _gAyaMistMouthAlternateUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_MIST_MOUTH_FRAME_WIDTH_WORDS, AYA_MIST_MOUTH_FRAME_HEIGHT_ROWS }, _gAyaMistMouthAlternatePixels },
    { GPU_IMAGE_UPLOAD_END },
};

GpuImageUpload* gAyaMistEyesRestSequence[]    = { _gAyaMistEyesOpenUploads, NULL };
GpuImageUpload* gAyaMistEyesClosingSequence[] = { _gAyaMistEyesOpenUploads, _gAyaMistEyesHalfClosedUploads, _gAyaMistEyesClosedUploads, NULL };
GpuImageUpload* gAyaMistEyesOpeningSequence[] = { _gAyaMistEyesClosedUploads, _gAyaMistEyesHalfClosedUploads, _gAyaMistEyesOpenUploads, NULL };
GpuImageUpload* gAyaMistEyesBlinkSequence[]   = {
    _gAyaMistEyesOpenUploads,
    _gAyaMistEyesHalfClosedUploads,
    _gAyaMistEyesClosedUploads,
    _gAyaMistEyesHalfClosedUploads,
    _gAyaMistEyesOpenUploads,
    NULL,
};
GpuImageUpload* gAyaMistMouthRestSequence[]      = { _gAyaMistMouthRestUploads, NULL };
GpuImageUpload* gAyaMistMouthAlternateSequence[] = { _gAyaMistMouthAlternateUploads, NULL };
