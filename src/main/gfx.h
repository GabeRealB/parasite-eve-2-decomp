#ifndef MAIN_PRIVATE_GFX_H
#define MAIN_PRIVATE_GFX_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

/// Color/light matrix written by Gfx_SetDefaultFlatLight / Gfx_SetLightAmbient.
extern MATRIX D_80074080;

/// Captures the displayed 320x240 16-bit frame in the area's RAM image slot.
///
/// Requires the selected stage's map overlay to be loaded, `stageId` in 1..5,
/// and `areaId` to select a nonempty entry in that stage's image-slot table.
/// The slot must provide a word-aligned 0x25800-byte frame followed by a
/// 0x10000-byte workspace. No index or extent checks are performed.
///
/// For the standard non-interlaced setup, `bufferIndex` is the environment
/// selector (0 or 1): capture uses its display area, at VRAM y=272 for 0 and
/// y=0 for 1. It waits for the GPU transfer before replacing the heap bases.
///
/// The workspace starts immediately after the frame and becomes the fixed
/// GPU primitive reservation and the saved whole auxiliary region.
/// `auxHeapOffsetBytes` is the active/saved auxiliary heap's byte offset within
/// that workspace, in [0, 0x10000]; its extent is 0x10000 minus the offset.
/// All current callers use `MEMORY_PRIMITIVE_HEAP_BYTES` (0x10000), leaving an
/// empty auxiliary heap. Smaller offsets overlap the primitive reservation.
/// Previous allocations in repurposed storage must no longer be live; this
/// configures the regions without initializing heap3 or the primitive cursor.
void gfxCaptureAreaFrame(s32 stageId, s32 areaId, s32 bufferIndex, s32 auxHeapOffsetBytes);

/// Uploads the area's captured 320x240 16-bit frame to the selected draw area.
///
/// Requires the selected stage's map overlay to be loaded, `stageId` in 1..5,
/// and a nonempty `areaId` entry with a readable, word-aligned 0x25800-byte
/// captured frame. No index or extent checks are performed.
///
/// With the standard non-interlaced setup, `bufferIndex` is 0 or 1, selecting
/// VRAM y=0 or y=272 respectively. This is the draw area of that environment
/// selector, opposite to the display area read by `gfxCaptureAreaFrame`.
/// The upload is queued without waiting; keep the frame storage intact until
/// the GPU transfer finishes. Heap configuration is unchanged.
void gfxRestoreAreaFrame(s32 stageId, s32 areaId, s32 bufferIndex);

void Gfx_InitCoordinateTrees(void);

void Gpu_InitDefaultLights(void);

#endif // MAIN_PRIVATE_GFX_H
