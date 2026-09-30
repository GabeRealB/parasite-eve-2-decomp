#ifndef MAIN_TMD_H
#define MAIN_TMD_H

#include "types.h"

#include "main/tmd_types.h"

/// Resident sentinel for the current attached-model list.
///
/// `next` points to the first `TmdObject.link`, and `prev` to the last. After
/// `Tmd_InitLists`, an empty list has `next == NULL` and `prev == &gTmdList`.
/// The first element points back to this sentinel; the last points forward to
/// `NULL`. Recover model bodies from element links with `PARENT_OF`; the
/// sentinel itself is not a `TmdObject`.
///
/// Successful attachment appends a body; creation alone does not link it.
/// Hidden models and models without primitive buffers remain linked, including
/// models awaiting delayed task release. Coordinate refresh visits every model;
/// drawing and buffer management apply their own filters. Unlink a body before
/// freeing it; the resident sentinel is never freed.
///
/// Saving and clearing the endpoints temporarily replaces the working list
/// without moving its bodies. Saved chains retain their back-links to this
/// sentinel and must stay linked and alive until the endpoints are restored.
extern TmdListNode gTmdList;

/// Cleared by Tmd_InitLists during system init.
extern s32 D_80071210;

void Tmd_InitLists(void);

/// Creates a hidden model object with a mutable copy of its source skeleton.
///
/// The object owns its per-part coordinate array and borrows `src`, which must
/// remain alive until the object is released. The skeleton is read only during
/// creation: each local matrix is copied and each parent index becomes a link,
/// with self-parented roots initially linked to the view coordinate.
///
/// Zero `bufferFlags` allocates and initializes both buffer halves. Nonzero
/// values defer allocation; bit 0 also disables missing-buffer recovery for
/// the object. These flags do not exempt an existing buffer from release.
/// Returns NULL if the object allocation fails. A buffer allocation failure
/// leaves a valid object with a NULL buffer for later allocation.
TmdObject* Tmd_Create(TmdSource* src, s32 bufferFlags);

/// Builds a model's primitives into one half of its buffer: it walks the model's
/// packet stream and runs the handler the record's opcode selects, and the
/// handler lays the record's packets out and fills in what the record carries.
///
/// A packet is not all per-frame. Its screen coordinates, its lit colours and its
/// link into the ordering table are the draw pass's to write as it transforms the
/// record, and what is left — a textured primitive's texture coordinates, and the
/// model's texture page and CLUT row added to the primitive's own — is settled
/// here, once. So the walk runs wherever a model's buffer is filled, and again
/// wherever the page or CLUT the model draws with changes.
///
/// It builds the half selected by `TmdObject.nextBufferHalf`, then toggles the
/// selector for the next build or draw pass. A caller that needs both halves
/// to carry the change calls it twice in a row. Its scratch frame is pushed on
/// `G_SCRATCH_HEAD` for the length of the walk, and the place the session is in
/// picks between the two handlers a record asking for a semi-transparent layer
/// has.
void tmdProcessStream(TmdObject* obj);

void Tmd_AllocMissingBuffers(void);

s32 Tmd_AllocBuffers(TmdObject* obj);

void Tmd_FreeBuffers(TmdObject* obj);

void Tmd_DrawFlaggedNodes(TmdObject* node);

void Tmd_DrawActiveNodes(TmdObject* node);

/// Draw-pass handler of a stream's gouraud-shaded textured-triangle records
/// (`0x38`, `0x3A`): each element contributes one triangle to the buffer half's
/// second region, with its corners projected and lit and the primitive linked
/// into the ordering table at the model's own offset.
///
/// The record's texture words belong to the pass that builds the primitive
/// (`gpStreamPrimGt3` writes them), so what is filled here is the triangle's
/// three corners and the colours they are lit from, which come from the three
/// normals the element names. An element whose projection the GTE rejects, or
/// whose triangle turns away, is stepped over rather than drawn, though its
/// packet slot is passed over either way, so the primitives stay aligned with
/// the elements that named them.
///
/// The record's `0x3A` form — `tmdDrawStreamGt3SemiTrans` — is the same body
/// reached with the semi-transparent shading constant, and this entry is the one
/// that picks the shading from `flags`. One further bit of `flags` picks between
/// the body's two copies of the walk, which keep opposite signs of the facing
/// result: a model drawn as a reflection asks for it, because a mirroring
/// transform reverses the model's faces.
u32* tmdDrawStreamGt3(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Draw handler of a stream's gouraud textured-quad records (`0x78`): each
/// element contributes one quad, projected and lit into the buffer slot the build
/// pass laid out for it, and linked into the ordering table at its own depth.
///
/// The element names a vertex and a normal per corner, so the quad is lit corner
/// by corner: three corners in one lighting step and the fourth in a step of its
/// own, each corner's colour being what its own normal gives under the model's
/// light. A corner the GTE reports off screen, or a quad the facing tests reject,
/// is not drawn — the packet is left out of the ordering table and the walk moves
/// on to the next element. The body carries a second copy of that walk for the
/// `0x10` bit of the object's flags, with the facing tests inverted, so a quad the
/// copy culls is one this one draws; what the object sets that bit for is not
/// established.
///
/// The packet's texture words, page and CLUT are the build pass's
/// (`gpStreamPrimGt4`); this pass writes the half a frame produces — the corner
/// coordinates, the corner colours, and the packet's length and primitive code.
/// That code is the semi-transparent one where the drawing object's flags ask for
/// it, which is what this handler reads `flags` for: the `0x7A` record's handler,
/// `tmdDrawStreamGt4SemiTrans`, shares this body and takes that code whatever the
/// flags say.
u32* tmdDrawStreamGt4(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's transform pre-pass records (`0xC8`): each element
/// contributes one transformed vertex to the buffer half, and the record builds
/// no primitive of its own.
///
/// An element names a vertex, a normal, and the two places in the buffer half
/// its results go: the vertex is projected into screen coordinates, the normal
/// is lit into a colour, and both are written where the element names. The
/// elements carry no colour word, so the lighting uses a fixed colour instead;
/// the record whose elements name one is `tmdXformStreamVertsElemColor`, and the
/// record that drops the lighting altogether is `gpXformStreamVertsUnlit`.
///
/// The projection is reused where consecutive elements name the same vertex, and
/// each vertex's depth goes to the per-vertex cache: the commands that build a
/// primitive from screen coordinates already in the buffer average those depths
/// to order it, and a vertex whose transform reported an error is stored with
/// its sign bit set, which is how those commands know the primitive cannot be
/// drawn. The record has no variant for `flags` to select, so it goes unread.
u32* tmdXformStreamVerts(TmdScratchModelBlock* ws, s32 flags, u32* stream);

#endif // MAIN_TMD_H
