#include "aya/aya.h"

#include <psyq/sys/types.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/gpu_image_upload.h"

/// Facial-frame dimensions in 16-bit VRAM words and rows.
enum {
    AYA_AKROPOLIS_EYES_FRAME_WIDTH_WORDS  = 25,
    AYA_AKROPOLIS_EYES_FRAME_HEIGHT_ROWS  = 16,
    AYA_AKROPOLIS_MOUTH_FRAME_WIDTH_WORDS = 14,
    AYA_AKROPOLIS_MOUTH_FRAME_HEIGHT_ROWS = 20,
};

/// Nineteen initial local transforms and parent indices copied into each player body.
///
/// Read-only pose and count tables retain their serialized positions in .data.
static const TmdBone _gAyaAkropolisBodyModelSkeleton[19] __attribute__((section(".data"))) = {
#include "assets/aya_akropolis_model_01A48_skeleton.inc"
};

/// Vertex counts for consecutive part-local groups in skeleton order, totaling 362.
static const u32 _gAyaAkropolisBodyModelPartVerts[ARRAY_SIZE(_gAyaAkropolisBodyModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_akropolis_model_01A48_partVerts.inc"
};

/// Writable body vertices in integer coordinates local to their skeletal parts.
static SVECTOR _gAyaAkropolisBodyModelVerts[362] = {
#include "assets/aya_akropolis_model_01A48_verts.inc"
};

/// Writable body lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gAyaAkropolisBodyModelNormals[383] = {
#include "assets/aya_akropolis_model_01A48_normals.inc"
};

/// Writable commands for nineteen skeletal groups and a final pre-transformed group.
///
/// Handler slots resolve in place; `TMD_STREAM_END` is the last word.
/// Projection commands populate the first region of each primitive-buffer half.
static u32 _gAyaAkropolisBodyModelStream[4017] = {
#include "assets/aya_akropolis_model_01A48_stream.inc"
};

TmdSource gAyaAkropolisBodyModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (260 * sizeof(POLY_GT3)) + (230 * sizeof(POLY_GT4)),
    .preXformRegionBytes = (80 * sizeof(POLY_GT3)) + (50 * sizeof(POLY_GT4)),
    .partCount           = ARRAY_SIZE(_gAyaAkropolisBodyModelSkeleton),
    .partVertexCounts    = _gAyaAkropolisBodyModelPartVerts,
    .verts               = _gAyaAkropolisBodyModelVerts,
    .normals             = _gAyaAkropolisBodyModelNormals,
    .skeleton            = _gAyaAkropolisBodyModelSkeleton,
    .stream              = _gAyaAkropolisBodyModelStream,
};

/// Initial local transform and self-parented root for hand slot 0 in either pair.
static const TmdBone _gAyaAkropolisHand0ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_akropolis_model_05AC4_skeleton.inc"
};

/// Vertex count of the shared hand's sole part: 23.
static const u32 _gAyaAkropolisHand0ModelPartVerts[ARRAY_SIZE(_gAyaAkropolisHand0ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_akropolis_model_05AC4_partVerts.inc"
};

/// Writable shared hand-0 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaAkropolisHand0ModelVerts[23] = {
#include "assets/aya_akropolis_model_05AC4_verts.inc"
};

/// Writable shared hand-0 lighting normals with twelve fractional bits.
static SVECTOR _gAyaAkropolisHand0ModelNormals[23] = {
#include "assets/aya_akropolis_model_05AC4_normals.inc"
};

/// Writable commands for four textured triangles and nineteen textured quads, ending in `TMD_STREAM_END`.
static u32 _gAyaAkropolisHand0ModelStream[166] = {
#include "assets/aya_akropolis_model_05AC4_stream.inc"
};

TmdSource gAyaAkropolisHand0Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (4 * sizeof(POLY_GT3)) + (19 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaAkropolisHand0ModelSkeleton),
    .partVertexCounts    = _gAyaAkropolisHand0ModelPartVerts,
    .verts               = _gAyaAkropolisHand0ModelVerts,
    .normals             = _gAyaAkropolisHand0ModelNormals,
    .skeleton            = _gAyaAkropolisHand0ModelSkeleton,
    .stream              = _gAyaAkropolisHand0ModelStream,
};

/// Initial local transform and self-parented root for first-pair hand slot 1.
static const TmdBone _gAyaAkropolisFirstPairHand1ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_akropolis_model_05F58_skeleton.inc"
};

/// Vertex count of the first-pair hand's sole part: 27.
static const u32 _gAyaAkropolisFirstPairHand1ModelPartVerts[ARRAY_SIZE(_gAyaAkropolisFirstPairHand1ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_akropolis_model_05F58_partVerts.inc"
};

/// Writable first-pair hand-1 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaAkropolisFirstPairHand1ModelVerts[27] = {
#include "assets/aya_akropolis_model_05F58_verts.inc"
};

/// Writable first-pair hand-1 lighting normals with twelve fractional bits.
static SVECTOR _gAyaAkropolisFirstPairHand1ModelNormals[27] = {
#include "assets/aya_akropolis_model_05F58_normals.inc"
};

/// Writable commands for two textured triangles and twenty-four textured quads, ending in `TMD_STREAM_END`.
static u32 _gAyaAkropolisFirstPairHand1ModelStream[189] = {
#include "assets/aya_akropolis_model_05F58_stream.inc"
};

TmdSource gAyaAkropolisFirstPairHand1Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (24 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaAkropolisFirstPairHand1ModelSkeleton),
    .partVertexCounts    = _gAyaAkropolisFirstPairHand1ModelPartVerts,
    .verts               = _gAyaAkropolisFirstPairHand1ModelVerts,
    .normals             = _gAyaAkropolisFirstPairHand1ModelNormals,
    .skeleton            = _gAyaAkropolisFirstPairHand1ModelSkeleton,
    .stream              = _gAyaAkropolisFirstPairHand1ModelStream,
};

/// Initial local transform and self-parented root for second-pair hand slot 1.
static const TmdBone _gAyaAkropolisSecondPairHand1ModelSkeleton[1] __attribute__((section(".data"))) = {
#include "assets/aya_akropolis_model_06408_skeleton.inc"
};

/// Vertex count of the second-pair hand's sole part: 23.
static const u32 _gAyaAkropolisSecondPairHand1ModelPartVerts[ARRAY_SIZE(_gAyaAkropolisSecondPairHand1ModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/aya_akropolis_model_06408_partVerts.inc"
};

/// Writable second-pair hand-1 vertices in integer coordinates local to the wrist.
static SVECTOR _gAyaAkropolisSecondPairHand1ModelVerts[23] = {
#include "assets/aya_akropolis_model_06408_verts.inc"
};

/// Writable second-pair hand-1 lighting normals with twelve fractional bits.
static SVECTOR _gAyaAkropolisSecondPairHand1ModelNormals[23] = {
#include "assets/aya_akropolis_model_06408_normals.inc"
};

/// Writable commands for four textured triangles and nineteen textured quads, ending in `TMD_STREAM_END`.
static u32 _gAyaAkropolisSecondPairHand1ModelStream[166] = {
#include "assets/aya_akropolis_model_06408_stream.inc"
};

TmdSource gAyaAkropolisSecondPairHand1Model = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (4 * sizeof(POLY_GT3)) + (19 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gAyaAkropolisSecondPairHand1ModelSkeleton),
    .partVertexCounts    = _gAyaAkropolisSecondPairHand1ModelPartVerts,
    .verts               = _gAyaAkropolisSecondPairHand1ModelVerts,
    .normals             = _gAyaAkropolisSecondPairHand1ModelNormals,
    .skeleton            = _gAyaAkropolisSecondPairHand1ModelSkeleton,
    .stream              = _gAyaAkropolisSecondPairHand1ModelStream,
};

static u_long _gAyaAkropolisEyesOpenPixels[] = {
#include "assets/aya_akropolis_image_066C4.inc"
};
/// One-copy upload list for open eyes: 50x16 8-bit texels.
///
/// Playback supplies the destination at texture U/V (0, 78). Records remain
/// writable; their payload must stay loaded until the GPU transfer completes.
static GpuImageUpload _gAyaAkropolisEyesOpenUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_AKROPOLIS_EYES_FRAME_WIDTH_WORDS, AYA_AKROPOLIS_EYES_FRAME_HEIGHT_ROWS }, _gAyaAkropolisEyesOpenPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long _gAyaAkropolisEyesHalfClosedPixels[] = {
#include "assets/aya_akropolis_image_06A04.inc"
};
/// One-copy upload list for half-closed eyes: 50x16 8-bit texels.
///
/// Placement, writable records and payload lifetime follow `_gAyaAkropolisEyesOpenUploads`.
static GpuImageUpload _gAyaAkropolisEyesHalfClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_AKROPOLIS_EYES_FRAME_WIDTH_WORDS, AYA_AKROPOLIS_EYES_FRAME_HEIGHT_ROWS }, _gAyaAkropolisEyesHalfClosedPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long _gAyaAkropolisEyesClosedPixels[] = {
#include "assets/aya_akropolis_image_06D44.inc"
};
/// One-copy upload list for closed eyes: 50x16 8-bit texels.
///
/// Placement, writable records and payload lifetime follow `_gAyaAkropolisEyesOpenUploads`.
static GpuImageUpload _gAyaAkropolisEyesClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_AKROPOLIS_EYES_FRAME_WIDTH_WORDS, AYA_AKROPOLIS_EYES_FRAME_HEIGHT_ROWS }, _gAyaAkropolisEyesClosedPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long _gAyaAkropolisMouthClosedPixels[] = {
#include "assets/aya_akropolis_image_07084.inc"
};
/// One-copy upload list for the resting, closed mouth: 28x20 8-bit texels.
///
/// Playback supplies the destination at texture U/V (12, 104), with an eight-tick
/// frame cadence. Records remain writable; keep the payload through GPU completion.
static GpuImageUpload _gAyaAkropolisMouthClosedUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_AKROPOLIS_MOUTH_FRAME_WIDTH_WORDS, AYA_AKROPOLIS_MOUTH_FRAME_HEIGHT_ROWS }, _gAyaAkropolisMouthClosedPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long _gAyaAkropolisMouthOpenPixels[] = {
#include "assets/aya_akropolis_image_072D4.inc"
};
/// One-copy upload list for the open mouth: 28x20 8-bit texels.
///
/// Placement, writable records and payload lifetime follow `_gAyaAkropolisMouthClosedUploads`.
static GpuImageUpload _gAyaAkropolisMouthOpenUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, AYA_AKROPOLIS_MOUTH_FRAME_WIDTH_WORDS, AYA_AKROPOLIS_MOUTH_FRAME_HEIGHT_ROWS }, _gAyaAkropolisMouthOpenPixels },
    { GPU_IMAGE_UPLOAD_END },
};

GpuImageUpload* const gAyaAkropolisEyesRestSequence[2] __attribute__((section(".data")))    = { _gAyaAkropolisEyesOpenUploads, NULL };
GpuImageUpload* const gAyaAkropolisEyesClosingSequence[4] __attribute__((section(".data"))) = { _gAyaAkropolisEyesOpenUploads, _gAyaAkropolisEyesHalfClosedUploads, _gAyaAkropolisEyesClosedUploads, NULL };
GpuImageUpload* const gAyaAkropolisEyesOpeningSequence[4] __attribute__((section(".data"))) = { _gAyaAkropolisEyesClosedUploads, _gAyaAkropolisEyesHalfClosedUploads, _gAyaAkropolisEyesOpenUploads, NULL };
GpuImageUpload* const gAyaAkropolisEyesBlinkSequence[6] __attribute__((section(".data")))   = {
    _gAyaAkropolisEyesOpenUploads,
    _gAyaAkropolisEyesHalfClosedUploads,
    _gAyaAkropolisEyesClosedUploads,
    _gAyaAkropolisEyesHalfClosedUploads,
    _gAyaAkropolisEyesOpenUploads,
    NULL,
};
GpuImageUpload* const gAyaAkropolisMouthRestSequence[2] __attribute__((section(".data"))) = { _gAyaAkropolisMouthClosedUploads, NULL };
GpuImageUpload* const gAyaAkropolisMouthOpenSequence[2] __attribute__((section(".data"))) = { _gAyaAkropolisMouthOpenUploads, NULL };
