#include "aya/aya.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/gpu_image_upload.h"

/// Facial-frame dimensions, in 16-bit VRAM words and rows.
enum {
    AYA_DRYFIELD_EYES_FRAME_WIDTH_WORDS  = 25,
    AYA_DRYFIELD_EYES_FRAME_HEIGHT_ROWS  = 16,
    AYA_DRYFIELD_MOUTH_FRAME_WIDTH_WORDS = 14,
    AYA_DRYFIELD_MOUTH_FRAME_HEIGHT_ROWS = 20,
};

/// Nineteen initial part transforms and parent indices copied into each player body.
///
/// Read-only pose and count tables retain their serialized positions in .data.
static const TmdBone _gAyaDryfieldBodyModelSkeleton[19] __attribute__((section(".data"))) = {
#include "assets/aya_dryfield_model_019A8_skeleton.inc"
};

/// Vertex counts for consecutive part-local groups in skeleton order, totaling 352.
static const u32 _gAyaDryfieldBodyModelPartVerts[ARRAY_SIZE(_gAyaDryfieldBodyModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_dryfield_model_019A8_partVerts.inc"
};

/// Writable body vertices in integer coordinates local to their skeletal parts.
static SVECTOR _gAyaDryfieldBodyModelVerts[352] = {
#include "assets/aya_dryfield_model_019A8_verts.inc"
};

/// Writable body lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gAyaDryfieldBodyModelNormals[373] = {
#include "assets/aya_dryfield_model_019A8_normals.inc"
};

/// Writable commands for nineteen skeletal groups and a final pre-transformed group.
///
/// First model creation resolves handler slots in place; `TMD_STREAM_END` is
/// the last word. Projection commands populate the pre-transformed region.
static u32 _gAyaDryfieldBodyModelStream[3829] = {
#include "assets/aya_dryfield_model_019A8_stream.inc"
};

TmdSource gAyaDryfieldBodyModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (236 * sizeof(POLY_GT3)) + (232 * sizeof(POLY_GT4)),
    .preXformRegionBytes = (66 * sizeof(POLY_GT3)) + (53 * sizeof(POLY_GT4)),
    .partCount           = ARRAY_SIZE(_gAyaDryfieldBodyModelSkeleton),
    .partVertexCounts    = _gAyaDryfieldBodyModelPartVerts,
    .verts               = _gAyaDryfieldBodyModelVerts,
    .normals             = _gAyaDryfieldBodyModelNormals,
    .skeleton            = _gAyaDryfieldBodyModelSkeleton,
    .stream              = _gAyaDryfieldBodyModelStream,
};

/// Initial local transform and self-parented root for first-pair hand slot 0.
static const TmdBone _gAyaDryfieldFirstPairHand0ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_dryfield_model_05734_skeleton.inc"
};

/// Vertex count of the hand's sole part: 23.
static const u32 _gAyaDryfieldFirstPairHand0ModelPartVerts[ARRAY_SIZE(_gAyaDryfieldFirstPairHand0ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_dryfield_model_05734_partVerts.inc"
};

/// Writable first-pair hand-0 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaDryfieldFirstPairHand0ModelVerts[23] = {
#include "assets/aya_dryfield_model_05734_verts.inc"
};

/// Writable first-pair hand-0 lighting normals with twelve fractional bits.
static SVECTOR _gAyaDryfieldFirstPairHand0ModelNormals[23] = {
#include "assets/aya_dryfield_model_05734_normals.inc"
};

/// Writable commands for two textured triangles and twenty textured quads, ending in `TMD_STREAM_END`.
static u32 _gAyaDryfieldFirstPairHand0ModelStream[161] = {
#include "assets/aya_dryfield_model_05734_stream.inc"
};

TmdSource gAyaDryfieldFirstPairHand0Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (20 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaDryfieldFirstPairHand0ModelSkeleton),
    .partVertexCounts    = _gAyaDryfieldFirstPairHand0ModelPartVerts,
    .verts               = _gAyaDryfieldFirstPairHand0ModelVerts,
    .normals             = _gAyaDryfieldFirstPairHand0ModelNormals,
    .skeleton            = _gAyaDryfieldFirstPairHand0ModelSkeleton,
    .stream              = _gAyaDryfieldFirstPairHand0ModelStream,
};

/// Initial local transform and self-parented root for second-pair hand slot 0.
///
/// Read-only source tables retain their serialized positions in the data section.
static const TmdBone _gAyaDryfieldSecondPairHand0ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_0019C_skeleton.inc"
};

/// Vertex count of the hand's sole part: 23.
static const u32 _gAyaDryfieldSecondPairHand0ModelPartVerts[ARRAY_SIZE(_gAyaDryfieldSecondPairHand0ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_mist_model_0019C_partVerts.inc"
};

/// Writable second-pair hand-0 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaDryfieldSecondPairHand0ModelVerts[23] = {
#include "assets/aya_mist_model_0019C_verts.inc"
};

/// Writable second-pair hand-0 lighting normals with twelve fractional bits and independent indices.
static SVECTOR _gAyaDryfieldSecondPairHand0ModelNormals[23] = {
#include "assets/aya_mist_model_0019C_normals.inc"
};

/// Writable commands for two textured triangles and twenty textured quads, ending in `TMD_STREAM_END`.
///
/// First model creation resolves each command's draw-handler slot in place.
static u32 _gAyaDryfieldSecondPairHand0ModelStream[161] = {
#include "assets/aya_mist_model_0019C_stream.inc"
};

TmdSource gAyaDryfieldSecondPairHand0Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (20 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaDryfieldSecondPairHand0ModelSkeleton),
    .partVertexCounts    = _gAyaDryfieldSecondPairHand0ModelPartVerts,
    .verts               = _gAyaDryfieldSecondPairHand0ModelVerts,
    .normals             = _gAyaDryfieldSecondPairHand0ModelNormals,
    .skeleton            = _gAyaDryfieldSecondPairHand0ModelSkeleton,
    .stream              = _gAyaDryfieldSecondPairHand0ModelStream,
};

/// Initial local transform and self-parented root for first-pair hand slot 1.
static const TmdBone _gAyaDryfieldFirstPairHand1ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_dryfield_model_05FF4_skeleton.inc"
};

/// Vertex count of the hand's sole part: 27.
static const u32 _gAyaDryfieldFirstPairHand1ModelPartVerts[ARRAY_SIZE(_gAyaDryfieldFirstPairHand1ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_dryfield_model_05FF4_partVerts.inc"
};

/// Writable first-pair hand-1 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaDryfieldFirstPairHand1ModelVerts[27] = {
#include "assets/aya_dryfield_model_05FF4_verts.inc"
};

/// Writable first-pair hand-1 lighting normals with twelve fractional bits.
static SVECTOR _gAyaDryfieldFirstPairHand1ModelNormals[27] = {
#include "assets/aya_dryfield_model_05FF4_normals.inc"
};

/// Writable commands for two textured triangles and twenty-four textured quads, ending in `TMD_STREAM_END`.
static u32 _gAyaDryfieldFirstPairHand1ModelStream[189] = {
#include "assets/aya_dryfield_model_05FF4_stream.inc"
};

TmdSource gAyaDryfieldFirstPairHand1Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaDryfieldFirstPairHand1ModelSkeleton),
    .partVertexCounts    = _gAyaDryfieldFirstPairHand1ModelPartVerts,
    .verts               = _gAyaDryfieldFirstPairHand1ModelVerts,
    .normals             = _gAyaDryfieldFirstPairHand1ModelNormals,
    .skeleton            = _gAyaDryfieldFirstPairHand1ModelSkeleton,
    .stream              = _gAyaDryfieldFirstPairHand1ModelStream,
};

/// Initial local transform and self-parented root for second-pair hand slot 1.
static const TmdBone _gAyaDryfieldSecondPairHand1ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_dryfield_model_064A4_skeleton.inc"
};

/// Vertex count of the hand's sole part: 23.
static const u32 _gAyaDryfieldSecondPairHand1ModelPartVerts[ARRAY_SIZE(_gAyaDryfieldSecondPairHand1ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_dryfield_model_064A4_partVerts.inc"
};

/// Writable second-pair hand-1 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaDryfieldSecondPairHand1ModelVerts[23] = {
#include "assets/aya_dryfield_model_064A4_verts.inc"
};

/// Writable second-pair hand-1 lighting normals with twelve fractional bits.
static SVECTOR _gAyaDryfieldSecondPairHand1ModelNormals[23] = {
#include "assets/aya_dryfield_model_064A4_normals.inc"
};

/// Writable commands for two textured triangles and twenty textured quads, ending in `TMD_STREAM_END`.
static u32 _gAyaDryfieldSecondPairHand1ModelStream[161] = {
#include "assets/aya_dryfield_model_064A4_stream.inc"
};

TmdSource gAyaDryfieldSecondPairHand1Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (20 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaDryfieldSecondPairHand1ModelSkeleton),
    .partVertexCounts    = _gAyaDryfieldSecondPairHand1ModelPartVerts,
    .verts               = _gAyaDryfieldSecondPairHand1ModelVerts,
    .normals             = _gAyaDryfieldSecondPairHand1ModelNormals,
    .skeleton            = _gAyaDryfieldSecondPairHand1ModelSkeleton,
    .stream              = _gAyaDryfieldSecondPairHand1ModelStream,
};

static u_long _gAyaDryfieldEyesOpenPixels[] = {
#include "assets/aya_dryfield_image_0674C.inc"
};
/// Open eyes: one copy and an upload terminator; playback rewrites the destination.
static GpuImageUpload _gAyaDryfieldEyesOpenUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_DRYFIELD_EYES_FRAME_WIDTH_WORDS, AYA_DRYFIELD_EYES_FRAME_HEIGHT_ROWS }, _gAyaDryfieldEyesOpenPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long _gAyaDryfieldEyesHalfClosedPixels[] = {
#include "assets/aya_dryfield_image_06A8C.inc"
};
/// Half-closed eyes: one copy and an upload terminator; playback rewrites the destination.
static GpuImageUpload _gAyaDryfieldEyesHalfClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_DRYFIELD_EYES_FRAME_WIDTH_WORDS, AYA_DRYFIELD_EYES_FRAME_HEIGHT_ROWS }, _gAyaDryfieldEyesHalfClosedPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long _gAyaDryfieldEyesClosedPixels[] = {
#include "assets/aya_dryfield_image_06DCC.inc"
};
/// Closed eyes: one copy and an upload terminator; playback rewrites the destination.
static GpuImageUpload _gAyaDryfieldEyesClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_DRYFIELD_EYES_FRAME_WIDTH_WORDS, AYA_DRYFIELD_EYES_FRAME_HEIGHT_ROWS }, _gAyaDryfieldEyesClosedPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long _gAyaDryfieldMouthRestPixels[] = {
#include "assets/aya_dryfield_image_0710C.inc"
};
/// Resting mouth: one copy and an upload terminator; playback rewrites the destination.
static GpuImageUpload _gAyaDryfieldMouthRestUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_DRYFIELD_MOUTH_FRAME_WIDTH_WORDS, AYA_DRYFIELD_MOUTH_FRAME_HEIGHT_ROWS }, _gAyaDryfieldMouthRestPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long _gAyaDryfieldMouthOpenPixels[] = {
#include "assets/aya_dryfield_image_0735C.inc"
};
/// Open mouth: one copy and an upload terminator; playback rewrites the destination.
static GpuImageUpload _gAyaDryfieldMouthOpenUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_DRYFIELD_MOUTH_FRAME_WIDTH_WORDS, AYA_DRYFIELD_MOUTH_FRAME_HEIGHT_ROWS }, _gAyaDryfieldMouthOpenPixels },
    { GPU_IMAGE_UPLOAD_END },
};

GpuImageUpload* const gAyaDryfieldEyesRestSequence[2] __attribute__((section(".data")))    = { _gAyaDryfieldEyesOpenUploads, NULL };
GpuImageUpload* const gAyaDryfieldEyesClosingSequence[4] __attribute__((section(".data"))) = { _gAyaDryfieldEyesOpenUploads, _gAyaDryfieldEyesHalfClosedUploads, _gAyaDryfieldEyesClosedUploads, NULL };
GpuImageUpload* const gAyaDryfieldEyesOpeningSequence[4] __attribute__((section(".data"))) = { _gAyaDryfieldEyesClosedUploads, _gAyaDryfieldEyesHalfClosedUploads, _gAyaDryfieldEyesOpenUploads, NULL };
GpuImageUpload* const gAyaDryfieldEyesBlinkSequence[6] __attribute__((section(".data")))   = {
    _gAyaDryfieldEyesOpenUploads,
    _gAyaDryfieldEyesHalfClosedUploads,
    _gAyaDryfieldEyesClosedUploads,
    _gAyaDryfieldEyesHalfClosedUploads,
    _gAyaDryfieldEyesOpenUploads,
    NULL,
};
GpuImageUpload* const gAyaDryfieldMouthRestSequence[2] __attribute__((section(".data"))) = { _gAyaDryfieldMouthRestUploads, NULL };
GpuImageUpload* const gAyaDryfieldMouthOpenSequence[2] __attribute__((section(".data"))) = { _gAyaDryfieldMouthOpenUploads, NULL };
