#include "aya/aya.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/collision.h"

extern GpuImageUpload* D_aya_10300_8011CD44[];

extern GpuImageUpload* D_aya_10300_8011CD5C[];

extern GpuImageUpload* D_aya_10300_8011CD64[];

/// Facial-frame dimensions, in 16-bit VRAM words and rows.
enum {
    AYA_10300_EYES_FRAME_WIDTH_WORDS  = 25,
    AYA_10300_EYES_FRAME_HEIGHT_ROWS  = 16,
    AYA_10300_MOUTH_FRAME_WIDTH_WORDS = 14,
    AYA_10300_MOUTH_FRAME_HEIGHT_ROWS = 20,
};

/// Nineteen initial part transforms and parent indices copied into each player body.
///
/// Read-only pose and count tables retain their serialized positions in .data.
static const TmdBone _gAya10300BodyModelSkeleton[19] __attribute__((section(".data"))) = {
#include "assets/aya_10300_model_019A8_skeleton.inc"
};

/// Vertex counts for consecutive part-local groups in skeleton order, totaling 352.
static const u32 _gAya10300BodyModelPartVerts[ARRAY_SIZE(_gAya10300BodyModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_10300_model_019A8_partVerts.inc"
};

/// Writable body vertices in integer coordinates local to their skeletal parts.
static SVECTOR _gAya10300BodyModelVerts[352] = {
#include "assets/aya_10300_model_019A8_verts.inc"
};

/// Writable body lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gAya10300BodyModelNormals[373] = {
#include "assets/aya_10300_model_019A8_normals.inc"
};

/// Writable commands for nineteen skeletal groups and a final pre-transformed group.
///
/// First model creation resolves handler slots in place; `TMD_STREAM_END` is
/// the last word. Projection commands populate the pre-transformed region.
static u32 _gAya10300BodyModelStream[3829] = {
#include "assets/aya_10300_model_019A8_stream.inc"
};

TmdSource gAya10300BodyModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (236 * sizeof(POLY_GT3)) + (232 * sizeof(POLY_GT4)),
    .preXformRegionBytes = (66 * sizeof(POLY_GT3)) + (53 * sizeof(POLY_GT4)),
    .partCount           = ARRAY_SIZE(_gAya10300BodyModelSkeleton),
    .partVertexCounts    = _gAya10300BodyModelPartVerts,
    .verts               = _gAya10300BodyModelVerts,
    .normals             = _gAya10300BodyModelNormals,
    .skeleton            = _gAya10300BodyModelSkeleton,
    .stream              = _gAya10300BodyModelStream,
};

/// Initial local transform and self-parented root for first-pair hand slot 0.
static const TmdBone _gAya10300FirstPairHand0ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_10300_model_05734_skeleton.inc"
};

/// Vertex count of the hand's sole part: 23.
static const u32 _gAya10300FirstPairHand0ModelPartVerts[ARRAY_SIZE(_gAya10300FirstPairHand0ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_10300_model_05734_partVerts.inc"
};

/// Writable first-pair hand-0 vertices in integer coordinates local to the wrist.
static SVECTOR _gAya10300FirstPairHand0ModelVerts[23] = {
#include "assets/aya_10300_model_05734_verts.inc"
};

/// Writable first-pair hand-0 lighting normals with twelve fractional bits.
static SVECTOR _gAya10300FirstPairHand0ModelNormals[23] = {
#include "assets/aya_10300_model_05734_normals.inc"
};

/// Writable commands for two textured triangles and twenty textured quads, ending in `TMD_STREAM_END`.
static u32 _gAya10300FirstPairHand0ModelStream[161] = {
#include "assets/aya_10300_model_05734_stream.inc"
};

TmdSource gAya10300FirstPairHand0Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (20 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAya10300FirstPairHand0ModelSkeleton),
    .partVertexCounts    = _gAya10300FirstPairHand0ModelPartVerts,
    .verts               = _gAya10300FirstPairHand0ModelVerts,
    .normals             = _gAya10300FirstPairHand0ModelNormals,
    .skeleton            = _gAya10300FirstPairHand0ModelSkeleton,
    .stream              = _gAya10300FirstPairHand0ModelStream,
};

static TmdBone _gAya10300Aya10200Model0019CSkeleton[1] = {
#include "assets/aya_10200_model_0019C_skeleton.inc"
};

static u32 _gAya10300Aya10200Model0019CPartVerts[1] = {
#include "assets/aya_10200_model_0019C_partVerts.inc"
};

static SVECTOR _gAya10300Aya10200Model0019CVerts[23] = {
#include "assets/aya_10200_model_0019C_verts.inc"
};

static SVECTOR _gAya10300Aya10200Model0019CNormals[23] = {
#include "assets/aya_10200_model_0019C_normals.inc"
};

static u32 _gAya10300Aya10200Model0019CStream[161] = {
#include "assets/aya_10200_model_0019C_stream.inc"
};

TmdSource D_aya_10300_8011B568 = {
    0,
    1120,
    0,
    1,
    _gAya10300Aya10200Model0019CPartVerts,
    _gAya10300Aya10200Model0019CVerts,
    _gAya10300Aya10200Model0019CNormals,
    _gAya10300Aya10200Model0019CSkeleton,
    _gAya10300Aya10200Model0019CStream,
};

/// Initial local transform and self-parented root for first-pair hand slot 1.
static const TmdBone _gAya10300FirstPairHand1ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_10300_model_05FF4_skeleton.inc"
};

/// Vertex count of the hand's sole part: 27.
static const u32 _gAya10300FirstPairHand1ModelPartVerts[ARRAY_SIZE(_gAya10300FirstPairHand1ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_10300_model_05FF4_partVerts.inc"
};

/// Writable first-pair hand-1 vertices in integer coordinates local to the wrist.
static SVECTOR _gAya10300FirstPairHand1ModelVerts[27] = {
#include "assets/aya_10300_model_05FF4_verts.inc"
};

/// Writable first-pair hand-1 lighting normals with twelve fractional bits.
static SVECTOR _gAya10300FirstPairHand1ModelNormals[27] = {
#include "assets/aya_10300_model_05FF4_normals.inc"
};

/// Writable commands for two textured triangles and twenty-four textured quads, ending in `TMD_STREAM_END`.
static u32 _gAya10300FirstPairHand1ModelStream[189] = {
#include "assets/aya_10300_model_05FF4_stream.inc"
};

TmdSource gAya10300FirstPairHand1Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAya10300FirstPairHand1ModelSkeleton),
    .partVertexCounts    = _gAya10300FirstPairHand1ModelPartVerts,
    .verts               = _gAya10300FirstPairHand1ModelVerts,
    .normals             = _gAya10300FirstPairHand1ModelNormals,
    .skeleton            = _gAya10300FirstPairHand1ModelSkeleton,
    .stream              = _gAya10300FirstPairHand1ModelStream,
};

/// Initial local transform and self-parented root for second-pair hand slot 1.
static const TmdBone _gAya10300SecondPairHand1ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_10300_model_064A4_skeleton.inc"
};

/// Vertex count of the hand's sole part: 23.
static const u32 _gAya10300SecondPairHand1ModelPartVerts[ARRAY_SIZE(_gAya10300SecondPairHand1ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_10300_model_064A4_partVerts.inc"
};

/// Writable second-pair hand-1 vertices in integer coordinates local to the wrist.
static SVECTOR _gAya10300SecondPairHand1ModelVerts[23] = {
#include "assets/aya_10300_model_064A4_verts.inc"
};

/// Writable second-pair hand-1 lighting normals with twelve fractional bits.
static SVECTOR _gAya10300SecondPairHand1ModelNormals[23] = {
#include "assets/aya_10300_model_064A4_normals.inc"
};

/// Writable commands for two textured triangles and twenty textured quads, ending in `TMD_STREAM_END`.
static u32 _gAya10300SecondPairHand1ModelStream[161] = {
#include "assets/aya_10300_model_064A4_stream.inc"
};

TmdSource gAya10300SecondPairHand1Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (20 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAya10300SecondPairHand1ModelSkeleton),
    .partVertexCounts    = _gAya10300SecondPairHand1ModelPartVerts,
    .verts               = _gAya10300SecondPairHand1ModelVerts,
    .normals             = _gAya10300SecondPairHand1ModelNormals,
    .skeleton            = _gAya10300SecondPairHand1ModelSkeleton,
    .stream              = _gAya10300SecondPairHand1ModelStream,
};

static u_long _gAya10300Image0674C[] = {
#include "assets/aya_10300_image_0674C.inc"
};
/// Open eyes: one copy and an upload terminator; playback rewrites the destination.
static GpuImageUpload _gAya10300EyesOpenUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10300_EYES_FRAME_WIDTH_WORDS, AYA_10300_EYES_FRAME_HEIGHT_ROWS }, _gAya10300Image0674C },
    { GP_IMG_REC_END },
};

static u_long _gAya10300Image06A8C[] = {
#include "assets/aya_10300_image_06A8C.inc"
};
/// Half-closed eyes: one copy and an upload terminator; playback rewrites the destination.
static GpuImageUpload _gAya10300EyesHalfClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10300_EYES_FRAME_WIDTH_WORDS, AYA_10300_EYES_FRAME_HEIGHT_ROWS }, _gAya10300Image06A8C },
    { GP_IMG_REC_END },
};

static u_long _gAya10300Image06DCC[] = {
#include "assets/aya_10300_image_06DCC.inc"
};
/// Closed eyes: one copy and an upload terminator; playback rewrites the destination.
static GpuImageUpload _gAya10300EyesClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10300_EYES_FRAME_WIDTH_WORDS, AYA_10300_EYES_FRAME_HEIGHT_ROWS }, _gAya10300Image06DCC },
    { GP_IMG_REC_END },
};

static u_long _gAya10300Image0710C[] = {
#include "assets/aya_10300_image_0710C.inc"
};
/// Resting mouth: one copy and an upload terminator; playback rewrites the destination.
static GpuImageUpload _gAya10300MouthRestUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10300_MOUTH_FRAME_WIDTH_WORDS, AYA_10300_MOUTH_FRAME_HEIGHT_ROWS }, _gAya10300Image0710C },
    { GP_IMG_REC_END },
};

static u_long _gAya10300Image0735C[] = {
#include "assets/aya_10300_image_0735C.inc"
};
/// Open mouth: one copy and an upload terminator; playback rewrites the destination.
static GpuImageUpload _gAya10300MouthOpenUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10300_MOUTH_FRAME_WIDTH_WORDS, AYA_10300_MOUTH_FRAME_HEIGHT_ROWS }, _gAya10300Image0735C },
    { GP_IMG_REC_END },
};

GpuImageUpload* const gAya10300EyesRestSequence[2] __attribute__((section(".data")))    = { _gAya10300EyesOpenUploads, NULL };
GpuImageUpload* const gAya10300EyesClosingSequence[4] __attribute__((section(".data"))) = { _gAya10300EyesOpenUploads, _gAya10300EyesHalfClosedUploads, _gAya10300EyesClosedUploads, NULL };
GpuImageUpload* const gAya10300EyesOpeningSequence[4] __attribute__((section(".data"))) = { _gAya10300EyesClosedUploads, _gAya10300EyesHalfClosedUploads, _gAya10300EyesOpenUploads, NULL };
GpuImageUpload*       D_aya_10300_8011CD44[]                                            = {
    _gAya10300EyesOpenUploads,
    _gAya10300EyesHalfClosedUploads,
    _gAya10300EyesClosedUploads,
    _gAya10300EyesHalfClosedUploads,
    _gAya10300EyesOpenUploads,
    NULL,
};
GpuImageUpload* D_aya_10300_8011CD5C[] = { _gAya10300MouthRestUploads, NULL };
GpuImageUpload* D_aya_10300_8011CD64[] = { _gAya10300MouthOpenUploads, NULL };
