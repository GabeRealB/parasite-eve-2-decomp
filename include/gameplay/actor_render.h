#ifndef GAMEPLAY_ACTOR_RENDER_H
#define GAMEPLAY_ACTOR_RENDER_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/gpu_image_upload.h"

#include "main/coord.h"
#include "main/task_types.h"

/// Queues texture copies after placing the first entry on an actor's texture page.
///
/// `actorTask` must be live with a TMD extra. A NULL `uploadList` returns 1
/// without accessing `textureRect`; a present list returns 0 regardless of SDK
/// transfer results. A present list must be writable and terminated as required
/// by `gpuUploadImages`, and `textureRect` must be readable for this call.
///
/// The input rectangle mixes units: X counts two positions per 16-bit VRAM
/// word, width counts VRAM words, and Y/height count rows. The first destination
/// becomes X = 384 + trunc((X + 1) / 2) + 64 * signed texture-page displacement,
/// Y = 256 + Y, with width and height copied verbatim. Stores retain the low
/// 16 bits. Later entries keep their destinations. The resulting rectangles
/// must fit VRAM and the payload must cover the replaced width and height;
/// pixel storage remains borrowed until GPU transfer completes.
s32 actorRenderUploadTexture(Task* actorTask, GpuImageUpload* uploadList, const RECT* textureRect);

/// Refreshes a coordinate's cached transform through its complete parent chain.
///
/// Writes `workm` and `composeStamp` using the current composition pass,
/// without advancing the pass counter. Nodes beneath `gGfxViewCoord` include
/// the view transform and produce local-to-view matrices. A parentless node
/// with a nonzero rebuild stamp keeps its caller-supplied `workm`.
/// Stored Euler angles are not applied; matrix coefficients have 12 fractional
/// bits and translations retain the local matrices' signed coordinate units.
///
/// `coord` must be non-NULL. It and its borrowed ancestors must remain writable
/// and live for this call, and their parent chain must be acyclic. Clear
/// `composeStamp` when changing a local matrix or parent. The cache does not
/// identify its composition root: invalidate affected caches before reusing
/// nodes composed in another space. GTE working registers are clobbered.
void actorRenderComposeCoord(GfxCoord* coord);

/// Refreshes every coordinate for this frame, then draws the models the
/// flagged pass draws.
void Gp_DrawActorTmdFlagged(GsOT* arg0);

/// Composes attached coordinates, then draws buffered models enabled for the active pass.
///
/// Refreshes every body in `gModelObjectCoordBodyList` and every model's
/// `partCount` coordinates in `gTmdList`, including models excluded from
/// drawing or without primitive buffers. Composition includes the full parent
/// chains. Both lists share one rebuild stamp and visit parity; the composition
/// counter advances once before drawing, even when both lists are empty.
///
/// `unusedOt` is ignored: the caller must select `gGpuCurrentOt` and establish
/// the depth shift and GTE projection/depth-average settings before this call.
/// Drawing follows `tmdDrawActiveModels`: a NULL buffer or
/// `TMD_OBJECT_SKIP_ACTIVE_DRAW` skips a model, while `TMD_OBJECT_FLAGGED_PASS`
/// does not affect selection. Drawn models toggle their primitive-buffer half.
///
/// List topology and resource bindings must stay fixed during the pass, and
/// bodies, borrowed parents and model resources must remain live. Parent
/// chains must be acyclic, and each model must own
/// its nonnegative `partCount` coordinates. Clear `composeStamp` after local
/// matrix or parent changes; caches composed with an excluded ancestor must
/// be invalidated before this full-chain pass. Model roots must supply the
/// intended view transform. Buffer capacities, stream bounds and scratch-stack
/// requirements follow `tmdDrawActiveModels`. Packet and OT storage must remain
/// valid until GPU use ends. GTE working registers are left changed.
void actorRenderComposeAndDrawActiveModels(GsOT* unusedOt);

#endif // GAMEPLAY_ACTOR_RENDER_H
