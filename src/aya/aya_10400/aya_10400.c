#include "aya/aya.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/collision.h"

/// Facial-frame dimensions in 16-bit VRAM words and rows.
enum {
    AYA_10400_EYES_FRAME_WIDTH_WORDS  = 25,
    AYA_10400_EYES_FRAME_HEIGHT_ROWS  = 16,
    AYA_10400_MOUTH_FRAME_WIDTH_WORDS = 14,
    AYA_10400_MOUTH_FRAME_HEIGHT_ROWS = 20,
};

/// Nineteen initial local transforms and parent indices copied into each player body.
///
/// Read-only pose and count tables retain their serialized positions in .data.
static const TmdBone _gAya10400BodyModelSkeleton[19] __attribute__((section(".data"))) = {
#include "assets/aya_10400_model_01A48_skeleton.inc"
};

/// Vertex counts for consecutive part-local groups in skeleton order, totaling 362.
static const u32 _gAya10400BodyModelPartVerts[ARRAY_SIZE(_gAya10400BodyModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_10400_model_01A48_partVerts.inc"
};

/// Writable body vertices in integer coordinates local to their skeletal parts.
static SVECTOR _gAya10400BodyModelVerts[362] = {
#include "assets/aya_10400_model_01A48_verts.inc"
};

/// Writable body lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gAya10400BodyModelNormals[383] = {
#include "assets/aya_10400_model_01A48_normals.inc"
};

/// Writable commands for nineteen skeletal groups and a final pre-transformed group.
///
/// Handler slots resolve in place; `TMD_STREAM_END` is the last word.
/// Projection commands populate the first region of each primitive-buffer half.
static u32 _gAya10400BodyModelStream[4017] = {
#include "assets/aya_10400_model_01A48_stream.inc"
};

TmdSource gAya10400BodyModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (260 * sizeof(POLY_GT3)) + (230 * sizeof(POLY_GT4)),
    .preXformRegionBytes = (80 * sizeof(POLY_GT3)) + (50 * sizeof(POLY_GT4)),
    .partCount           = ARRAY_SIZE(_gAya10400BodyModelSkeleton),
    .partVertexCounts    = _gAya10400BodyModelPartVerts,
    .verts               = _gAya10400BodyModelVerts,
    .normals             = _gAya10400BodyModelNormals,
    .skeleton            = _gAya10400BodyModelSkeleton,
    .stream              = _gAya10400BodyModelStream,
};

/// Initial local transform and self-parented root for hand slot 0 in either pair.
static const TmdBone _gAya10400Hand0ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_10400_model_05AC4_skeleton.inc"
};

/// Vertex count of the shared hand's sole part: 23.
static const u32 _gAya10400Hand0ModelPartVerts[ARRAY_SIZE(_gAya10400Hand0ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_10400_model_05AC4_partVerts.inc"
};

/// Writable shared hand-0 vertices in integer coordinates local to the wrist.
static SVECTOR _gAya10400Hand0ModelVerts[23] = {
#include "assets/aya_10400_model_05AC4_verts.inc"
};

/// Writable shared hand-0 lighting normals with twelve fractional bits.
static SVECTOR _gAya10400Hand0ModelNormals[23] = {
#include "assets/aya_10400_model_05AC4_normals.inc"
};

/// Writable commands for four textured triangles and nineteen textured quads, ending in `TMD_STREAM_END`.
static u32 _gAya10400Hand0ModelStream[166] = {
#include "assets/aya_10400_model_05AC4_stream.inc"
};

TmdSource gAya10400Hand0Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (4 * sizeof(POLY_GT3)) + (19 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAya10400Hand0ModelSkeleton),
    .partVertexCounts    = _gAya10400Hand0ModelPartVerts,
    .verts               = _gAya10400Hand0ModelVerts,
    .normals             = _gAya10400Hand0ModelNormals,
    .skeleton            = _gAya10400Hand0ModelSkeleton,
    .stream              = _gAya10400Hand0ModelStream,
};

/// Initial local transform and self-parented root for first-pair hand slot 1.
static const TmdBone _gAya10400FirstPairHand1ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_10400_model_05F58_skeleton.inc"
};

/// Vertex count of the first-pair hand's sole part: 27.
static const u32 _gAya10400FirstPairHand1ModelPartVerts[ARRAY_SIZE(_gAya10400FirstPairHand1ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_10400_model_05F58_partVerts.inc"
};

/// Writable first-pair hand-1 vertices in integer coordinates local to the wrist.
static SVECTOR _gAya10400FirstPairHand1ModelVerts[27] = {
#include "assets/aya_10400_model_05F58_verts.inc"
};

/// Writable first-pair hand-1 lighting normals with twelve fractional bits.
static SVECTOR _gAya10400FirstPairHand1ModelNormals[27] = {
#include "assets/aya_10400_model_05F58_normals.inc"
};

/// Writable commands for two textured triangles and twenty-four textured quads, ending in `TMD_STREAM_END`.
static u32 _gAya10400FirstPairHand1ModelStream[189] = {
#include "assets/aya_10400_model_05F58_stream.inc"
};

TmdSource gAya10400FirstPairHand1Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAya10400FirstPairHand1ModelSkeleton),
    .partVertexCounts    = _gAya10400FirstPairHand1ModelPartVerts,
    .verts               = _gAya10400FirstPairHand1ModelVerts,
    .normals             = _gAya10400FirstPairHand1ModelNormals,
    .skeleton            = _gAya10400FirstPairHand1ModelSkeleton,
    .stream              = _gAya10400FirstPairHand1ModelStream,
};

/// Initial local transform and self-parented root for second-pair hand slot 1.
static const TmdBone _gAya10400SecondPairHand1ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_10400_model_06408_skeleton.inc"
};

/// Vertex count of the second-pair hand's sole part: 23.
static const u32 _gAya10400SecondPairHand1ModelPartVerts[ARRAY_SIZE(_gAya10400SecondPairHand1ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_10400_model_06408_partVerts.inc"
};

/// Writable second-pair hand-1 vertices in integer coordinates local to the wrist.
static SVECTOR _gAya10400SecondPairHand1ModelVerts[23] = {
#include "assets/aya_10400_model_06408_verts.inc"
};

/// Writable second-pair hand-1 lighting normals with twelve fractional bits.
static SVECTOR _gAya10400SecondPairHand1ModelNormals[23] = {
#include "assets/aya_10400_model_06408_normals.inc"
};

/// Writable commands for four textured triangles and nineteen textured quads, ending in `TMD_STREAM_END`.
static u32 _gAya10400SecondPairHand1ModelStream[166] = {
#include "assets/aya_10400_model_06408_stream.inc"
};

TmdSource gAya10400SecondPairHand1Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (4 * sizeof(POLY_GT3)) + (19 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAya10400SecondPairHand1ModelSkeleton),
    .partVertexCounts    = _gAya10400SecondPairHand1ModelPartVerts,
    .verts               = _gAya10400SecondPairHand1ModelVerts,
    .normals             = _gAya10400SecondPairHand1ModelNormals,
    .skeleton            = _gAya10400SecondPairHand1ModelSkeleton,
    .stream              = _gAya10400SecondPairHand1ModelStream,
};

static u_long _gAya10400Image066C4[] = {
#include "assets/aya_10400_image_066C4.inc"
};
/// One-copy upload list for open eyes: 50x16 8-bit texels.
///
/// Playback supplies the destination at texture U/V (0, 78). Records remain
/// writable; their payload must stay loaded until the GPU transfer completes.
static GpuImageUpload _gAya10400EyesOpenUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10400_EYES_FRAME_WIDTH_WORDS, AYA_10400_EYES_FRAME_HEIGHT_ROWS }, _gAya10400Image066C4 },
    { GP_IMG_REC_END },
};

static u_long _gAya10400Image06A04[] = {
#include "assets/aya_10400_image_06A04.inc"
};
/// One-copy upload list for half-closed eyes: 50x16 8-bit texels.
///
/// Placement, writable records and payload lifetime follow `_gAya10400EyesOpenUploads`.
static GpuImageUpload _gAya10400EyesHalfClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10400_EYES_FRAME_WIDTH_WORDS, AYA_10400_EYES_FRAME_HEIGHT_ROWS }, _gAya10400Image06A04 },
    { GP_IMG_REC_END },
};

static u_long _gAya10400Image06D44[] = {
#include "assets/aya_10400_image_06D44.inc"
};
/// One-copy upload list for closed eyes: 50x16 8-bit texels.
///
/// Placement, writable records and payload lifetime follow `_gAya10400EyesOpenUploads`.
static GpuImageUpload _gAya10400EyesClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10400_EYES_FRAME_WIDTH_WORDS, AYA_10400_EYES_FRAME_HEIGHT_ROWS }, _gAya10400Image06D44 },
    { GP_IMG_REC_END },
};

static u_long _gAya10400Image07084[] = {
#include "assets/aya_10400_image_07084.inc"
};
/// One-copy upload list for the resting, closed mouth: 28x20 8-bit texels.
///
/// Playback supplies the destination at texture U/V (12, 104), with an eight-tick
/// frame cadence. Records remain writable; keep the payload through GPU completion.
static GpuImageUpload _gAya10400MouthClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10400_MOUTH_FRAME_WIDTH_WORDS, AYA_10400_MOUTH_FRAME_HEIGHT_ROWS }, _gAya10400Image07084 },
    { GP_IMG_REC_END },
};

static u_long _gAya10400Image072D4[] = {
#include "assets/aya_10400_image_072D4.inc"
};
/// One-copy upload list for the open mouth: 28x20 8-bit texels.
///
/// Placement, writable records and payload lifetime follow `_gAya10400MouthClosedUploads`.
static GpuImageUpload _gAya10400MouthOpenUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_10400_MOUTH_FRAME_WIDTH_WORDS, AYA_10400_MOUTH_FRAME_HEIGHT_ROWS }, _gAya10400Image072D4 },
    { GP_IMG_REC_END },
};

GpuImageUpload* const gAya10400EyesRestSequence[2] __attribute__((section(".data")))    = { _gAya10400EyesOpenUploads, NULL };
GpuImageUpload* const gAya10400EyesClosingSequence[4] __attribute__((section(".data"))) = { _gAya10400EyesOpenUploads, _gAya10400EyesHalfClosedUploads, _gAya10400EyesClosedUploads, NULL };
GpuImageUpload* const gAya10400EyesOpeningSequence[4] __attribute__((section(".data"))) = { _gAya10400EyesClosedUploads, _gAya10400EyesHalfClosedUploads, _gAya10400EyesOpenUploads, NULL };
GpuImageUpload* const gAya10400EyesBlinkSequence[6] __attribute__((section(".data")))   = {
    _gAya10400EyesOpenUploads,
    _gAya10400EyesHalfClosedUploads,
    _gAya10400EyesClosedUploads,
    _gAya10400EyesHalfClosedUploads,
    _gAya10400EyesOpenUploads,
    NULL,
};
GpuImageUpload* const gAya10400MouthRestSequence[2] __attribute__((section(".data"))) = { _gAya10400MouthClosedUploads, NULL };
GpuImageUpload* const gAya10400MouthOpenSequence[2] __attribute__((section(".data"))) = { _gAya10400MouthOpenUploads, NULL };
