#ifndef GAMEPLAY_MODEL_OBJECTS_H
#define GAMEPLAY_MODEL_OBJECTS_H

struct Task;

#include "types.h"

#include "gameplay/display.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Resident sentinel for attached task-owned coordinate bodies.
///
/// `next` points to the first `ModelObjectCoordBody.link`, and `prev` to the
/// last. Initialization leaves `next == NULL` and `prev` pointing to this
/// sentinel. The first element points back to the sentinel; the last points
/// forward to `NULL`. Recover bodies with `PARENT_OF`; the sentinel is a bare
/// `TmdListNode`, never a coordinate body.
///
/// Successful attachment appends a body. Each model draw pass refreshes its
/// single coordinate before visiting model parts; this list emits no primitives.
/// Bodies must remain at their allocated addresses and be unlinked before
/// release. The resident sentinel is never freed.
///
/// Stashing saves and clears this list's endpoints together with `gTmdList`.
/// Saved elements retain their back-links to this sentinel and must stay linked
/// and alive until the endpoints are restored. Only one stash may be outstanding.
extern TmdListNode gModelObjectCoordBodyList;

/// Creates a model with primitive buffers and attaches it to a task.
///
/// `task` must have no existing body; success appends the model to `gTmdList`
/// and stores it in `extra.tmd` with `bodyKind` `TASK_BODY_TMD`. The task owns
/// the allocation and coordinates; source geometry remains borrowed. Source
/// requirements and buffer-allocation behavior follow `tmdCreateModel` with
/// zero buffer flags: auxiliary allocation failure can leave a live model
/// with a NULL primitive buffer. The model starts excluded from active drawing.
/// Returns NULL on primary allocation failure, leaving task body fields
/// unchanged. Unlink the model before releasing it with `modelObjectFreeTmd`.
TmdObject* modelObjectAttachTmd(Task* task, TmdSource* source);

/// Creates and attaches one task-owned coordinate body with an identity transform.
///
/// `task` must have no existing body. Success appends the allocation to
/// `gModelObjectCoordBodyList` and stores it in `extra.coordBody` with
/// `bodyKind` `TASK_BODY_COORD`. Its embedded coordinate initially has zero
/// translation and rotation angles, a dirty composition cache, and
/// `gGfxViewCoord` as its borrowed parent. Model draw passes refresh this
/// coordinate but emit no primitives for the body.
///
/// Returns NULL on allocation failure, leaving task body fields unchanged.
/// Keep the body at its allocated address and its parent alive while composed.
/// Unlink it before releasing it with `modelObjectFreeCoordBody`.
ModelObjectCoordBody* modelObjectAttachCoordBody(Task* task);

/// Creates and attaches a task-owned model with primitive-buffer creation options.
///
/// The attachment, source and lifetime contract is `modelObjectAttachTmd`'s.
/// `bufferFlags` is the signed s32 creation word passed to `tmdCreateModel`,
/// separate from `TmdObject.flags`: zero allocates and builds both buffer
/// halves; every nonzero value defers them, and bit 0 also suppresses
/// automatic missing-buffer recovery. Returns NULL only when primary
/// allocation fails, leaving the task's body fields unchanged.
TmdObject* modelObjectAttachTmdWithBufferFlags(Task* task, TmdSource* source, s32 bufferFlags);

/// Unlinks an attached model from the live model list (`gTmdList`).
///
/// `node` is that model's `link` and is on the live list. It is not the
/// sentinel, a coordinate-body link, or a link already removed from the live
/// list. A stashed chain still points back at this sentinel, but the live
/// endpoints no longer name it, so a stashed link is not an argument.
/// Neighbors are updated, and the sentinel's `prev` is updated when `node` is
/// the tail. `node`'s own `next` and `prev` are left unchanged. The model
/// stays allocated; release it with `modelObjectFreeTmd`. This does not change
/// the owning task's body pointer or body kind.
void modelObjectUnlinkTmd(TmdListNode* node);

/// Releases a detached task-owned model and any primitive buffer it owns.
///
/// `model` is the live object `tmdCreateModel` returned, including one attached by
/// `modelObjectAttachTmd` or `modelObjectAttachTmdWithBufferFlags`. It is not `NULL`. That address is the
/// primary-heap allocation, so releasing it also ends the owned coordinate
/// tail. Borrowed source geometry and the light and colour matrices stay with
/// their owners.
///
/// Unlink it from `gTmdList` first, keep it out of any stashed model list, and
/// end borrows of its part coordinates and uses of its buffer. A buffer, when
/// present, is the original auxiliary-heap allocation and is released before
/// the body; a `NULL` buffer is left alone. The caller owns the task's
/// body-pointer and body-kind bookkeeping. `modelObjectFreeCoordBody` releases
/// a coordinate body instead.
///
/// Releasing a buffer selects the auxiliary heap. Releasing the body then
/// selects the primary heap and leaves it selected.
void modelObjectFreeTmd(TmdObject* model);

/// Unlinks a coordinate body from the live refresh list (`gModelObjectCoordBodyList`).
///
/// `node` is that body's `link` and is on the live list. It is not the
/// sentinel, a model link, or a link already removed from the live list. A
/// stashed chain still points back at this sentinel, but the live endpoints no
/// longer name it, so a stashed link is not an argument. Neighbors are
/// updated, and the sentinel's `prev` is updated when `node` is the tail.
/// `node`'s own `next` and `prev` are left unchanged. The body stays
/// allocated; release it with `modelObjectFreeCoordBody`. This does not change
/// the owning task's body pointer or body kind.
void modelObjectUnlinkCoordBody(TmdListNode* node);

/// Releases a detached task-owned coordinate body to the primary heap.
///
/// `body` must be `NULL` or the original live allocation from `modelObjectAttachCoordBody`.
/// Before releasing a non-null body, unlink it from `gModelObjectCoordBodyList`
/// and ensure no stashed list still contains it. This also ends the embedded
/// coordinate's lifetime; borrowed parent coordinates are not released.
/// The caller owns the task's body-pointer and body-kind bookkeeping.
///
/// The primary heap becomes active even for `NULL` and remains selected.
void modelObjectFreeCoordBody(ModelObjectCoordBody* body);

void Gp_DrawDisp2dOt(struct Task* unused);

/// Culls and links pre-transformed flat quads from TMD stream opcode `0x45`.
///
/// `elements` starts after the record header; `workspace->elemCount` is 0..65535
/// and `elemStride` counts u32 words. The first four u16 values of each element
/// are depth-cache byte references. Clearing bits 0..1 and dividing by four
/// selects `szTable` entries. Each index must be below 1024 and initialized by
/// an earlier projection in this draw walk; masking does not check bounds.
/// The low bits' meaning and full element extent are unproven. Construction
/// reads colour from word 2, so each element must contain at least three words.
///
/// `preXformWrite` addresses one word-aligned `POLY_F4` per element in the chosen
/// buffer half's first region. Construction supplies its length, code and colour;
/// projection commands must supply all four screen-coordinate pairs in pixels.
/// Positive NCLIP area for vertices 0,1,2 accepts the facing test. Otherwise,
/// negative area for vertices 1,2,3 accepts it. `objectFlags` is unused, including
/// `TMD_OBJECT_REVERSE_CULLING`. Any corner with `TMD_VERTEX_DEPTH_INVALID`
/// rejects the quad. Only the packet's DMA link is changed; its length, code,
/// colour and coordinates are preserved.
///
/// Loads the four cached depths into SZ0..SZ3 and runs AVSZ4 with the current
/// ZSF4 scale. Unsigned OTZ is scaled by `gDisplayState.otDepthShift`, divided
/// by 16 and wrapped to 0..1023 relative to `workspace->ot`, which already
/// includes the model's signed offset. The selected OT must contain that entry.
///
/// Returns `elements + initial elemCount * elemStride` and advances
/// `preXformWrite` by that count of packets even when culled. Leaves `elemCount`
/// at -1 and reuses `gteResult` for facing and OTZ. Workspace, payload and depth
/// cache are borrowed for this draw walk. Linked packets and the OT must remain
/// alive until the GPU finishes consuming them.
u32* tmdDrawStreamPrimF4PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Culls and links pre-transformed flat triangles from TMD stream opcode `0x5`.
///
/// `elements` starts after the record header; `workspace->elemCount` is 0..65535
/// and `elemStride` counts u32 words. Each element's first three u16 values are
/// depth-cache byte references: clearing their low two bits and dividing by four
/// selects `szTable` entries. The low bits' meaning and the complete element
/// layout are unproven; the construction handler reads colour from word 2, so
/// the stride must be at least three words. Each selected cache index must be
/// below 1024 and initialized by an earlier projection in this draw walk.
///
/// `preXformWrite` addresses one word-aligned `POLY_F3` per element in the chosen
/// buffer half's first region. Construction supplies its length, code and colour;
/// projection commands must supply its three screen-coordinate pairs in pixels.
/// Only negative NCLIP area survives, independent of `objectFlags`, which is
/// unused, including `TMD_OBJECT_REVERSE_CULLING`. A corner depth carrying
/// `TMD_VERTEX_DEPTH_INVALID` denotes failed projection and rejects the triangle.
/// Linking preserves the packet's length, code, colour and coordinates.
///
/// The retained depth calculation loads corners 0..2 into SZ0..SZ2, then runs
/// AVSZ3, which reads SZ1..SZ3 with the current ZSF3 scale. Thus sorting uses
/// corners 1 and 2 plus the prior SZ3. The unsigned
/// OTZ is scaled by `gDisplayState.otDepthShift`, divided by 16 and wrapped to
/// 0..1023 relative to `workspace->ot`, which already includes the model offset.
/// The selected OT must contain that entry; this handler performs no bounds check.
///
/// Returns `elements + initial elemCount * elemStride` and advances
/// `preXformWrite` by that count of packets even when culled. Leaves `elemCount`
/// at -1 and reuses `gteResult` for facing and OTZ. Workspace, payload and depth
/// cache are borrowed for this draw walk. Linked packets and the OT must remain
/// alive until the GPU finishes consuming them.
u32* tmdDrawStreamPrimF3PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Culls and links pre-transformed textured triangles with an environment layer.
///
/// Resolving stream opcode `0x4039` selects this callback outside stage 2,
/// area 16. `objectFlags` is ignored, including reverse-culling and transparency
/// flags. `elements` starts after the three-word record header; the caller sets
/// `workspace->elemCount` (0..65535) and `elemStride` in u32 words. Each element's
/// first three u16 values are depth-cache byte references: clear bits 0..1 and
/// divide by four to select `szTable` entries. Their low bits' meaning and the
/// complete payload extent are unproven. Construction reads through word 4,
/// so each element needs at least five words. Every cache index must be below
/// 1024 and initialized by an earlier projection in this draw walk.
///
/// `preXformWrite` addresses two word-aligned `POLY_GT3` slots per element in
/// the selected buffer half's first region: environment layer first, opaque
/// base second. Projection supplies both packets' signed screen XY in pixels,
/// lit colours and the layer's U/V in texels. The layer's code/p1/p2 bytes must
/// initially hold 0/1 second-page markers, replaced by projection each draw.
/// Positive NCLIP area of its three corners accepts the facing test; zero or
/// negative area, or any `TMD_VERTEX_DEPTH_INVALID` corner, rejects both packets.
///
/// With no markers, the layer uses fixed page `0x137`; any marker selects
/// `0x139`. These are 15-bit additive pages at VRAM (448,256) and (576,256).
/// On the second page, marked corners retain U; unmarked corners subtract 128
/// when U >= 128 and clamp to zero otherwise. V and the fixed construction CLUT
/// remain intact. Accepted packets receive length 9 and commands `0x36` for
/// the layer and `0x34` for the base; the base's texture fields are preserved.
///
/// Loads cached depths into SZ1..SZ3 and runs AVSZ3 with the current ZSF3 scale.
/// Unsigned OTZ is shifted by `gDisplayState.otDepthShift`, divided by 16 and
/// wrapped to 0..1023 relative to `workspace->ot`, already displaced by the
/// model's signed offset. The selected OT must contain that entry. Links the
/// layer first and base second, so the head-inserted base precedes the layer.
///
/// Advances `preXformWrite` by one pair per element even when culled and returns
/// `elements + initial elemCount * elemStride`; consumes the count to -1 even
/// for an empty record. Reuses `gteResult` for facing and OTZ. Workspace, payload
/// and depth cache are borrowed for the draw walk. Packets and OT must remain
/// alive until the GPU finishes consuming them; no pointers are retained here.
u32* tmdDrawStreamPrimGt3PreXformEnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Culls and links pre-transformed textured quads with an additive environment layer.
///
/// Draw callback for opcode `0x4079` outside stage 2, area 16; that location
/// selects `tmdDrawStreamPrimGt4PreXformOffsetLayer`. `objectFlags` is ignored,
/// including transparency and reverse-culling bits. `elements` starts after
/// the three-word record header. `workspace->elemCount` initially counts
/// 0..65535 elements; `elemStride` counts u32 words. Drawing reads each element's
/// first four u16 depth references. Clear their low two bits and divide by four
/// to index `szTable`; the low bits' role is unproven. Each reference must select
/// an entry below 1024 initialized by an earlier projection in this draw walk.
/// Construction reads through word 4, requiring at least five words per element;
/// the complete payload extent is not established here.
///
/// `preXformWrite` addresses two writable, word-aligned `POLY_GT4` packets per
/// element in the selected buffer half's first region: environment layer first,
/// opaque model-texture base second. Earlier commands supply signed screen XY
/// in pixels, lit colours, layer U/V and 0/1 second-page markers in the layer's
/// `code/p1/p2/p3` bytes. Buffers, depth cache and workspace are borrowed.
///
/// Accepts positive NCLIP for corners 0/1/2 and negative for 1/2/3. If only one
/// half survives, copies XY 1 to 0 or 2 to 3 in both packets to fold away the
/// other half. The first copy occurs even if both halves are rejected. Depth
/// sorting still uses AVSZ4 over all four original cached depths and rejects
/// any `TMD_VERTEX_DEPTH_INVALID` entry, including a folded-away corner.
///
/// With no markers, the layer samples the direct-colour page at VRAM (448,256).
/// Any marker selects (576,256); unmarked U >= 128 subtracts 128 texels, and
/// smaller U clamps to zero. Marked U was already rebased by projection; V stays
/// unchanged. Writes 12-word packet lengths and commands 0x3E/0x3C, replacing
/// the first marker with the layer command. Both packets use the same OT index:
/// unsigned OTZ shifted left by `gDisplayState.otDepthShift`, then right by four,
/// wrapped to 0..1023 relative to the object's displaced `ot`. That table must
/// contain the selected entry. Head insertion draws the opaque base first.
///
/// Advances past both packet slots even for culled elements and returns the
/// word after the payload, leaving any stream marker unconsumed. Decrements
/// `elemCount` to -1, updates `preXformWrite`, reuses `gteResult`, and clobbers
/// the GTE screen/depth FIFOs, MAC0, OTZ and FLAG. No pointers are retained.
u32* tmdDrawStreamPrimGt4PreXformEnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Culls and links pre-transformed textured triangles with an object-offset layer.
///
/// Resolution selects this draw callback for opcode `0x4039` in stage 2, area
/// 16; other locations select `tmdDrawStreamPrimGt3PreXformEnvLayer`.
/// `objectFlags` is ignored, including transparency and reverse-culling bits.
/// `elements` starts after the three-word record header. The caller supplies
/// `workspace->elemCount` (0..65535) and `elemStride` in u32 words. The first
/// three u16 values of each element are depth-cache byte references: clear
/// bits 0..1 and divide by four to select `szTable` entries. Those low bits'
/// role is unproven. Each index must be below 1024 and initialized by an earlier
/// projection in this draw walk. Drawing reads six payload bytes; construction
/// reads through word 4, requiring at least five words, not a proven full extent.
///
/// `preXformWrite` supplies two writable, word-aligned `POLY_GT3` slots per
/// element in the selected buffer half's first region: blended layer first,
/// opaque base second. Earlier commands supply signed screen XY in pixels
/// and lit colours. Construction supplies both textures, applying the object's
/// layer page/CLUT offsets to the first packet and base offsets to the second.
/// This handler preserves those colours, coordinates, U/V, pages and CLUTs.
/// Only positive NCLIP area of the layer's corners passes. Zero/negative area
/// or any `TMD_VERTEX_DEPTH_INVALID` corner rejects both packets.
///
/// Loads screen Z into SZ1..SZ3 and runs AVSZ3 with the current ZSF3 scale.
/// Accepted packets receive nine-word DMA lengths and command bytes `0x36`
/// (blended layer) and `0x34` (opaque base). Unsigned OTZ is shifted by
/// `gDisplayState.otDepthShift`, divided by 16 and wrapped to 0..1023 relative
/// to `workspace->ot`, already displaced by the object's signed tag offset.
/// The selected OT must contain that entry. Links the layer then the base,
/// so head insertion draws the opaque base before the blended layer.
///
/// Advances `preXformWrite` by one 80-byte pair per element even when culled,
/// returns `elements + initial elemCount * elemStride`, and leaves `elemCount`
/// at -1 even for an empty record. Reuses `gteResult` for facing and OTZ.
/// Workspace, stream and depth cache are borrowed for the draw walk; no pointer
/// is retained here. Linked packets and OT must live until GPU consumption ends.
u32* tmdDrawStreamPrimGt3PreXformOffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Culls and links pre-transformed textured quads with an object-offset layer.
///
/// Resolution selects this draw callback for opcode `0x4079` in stage 2, area
/// 16; other locations select `tmdDrawStreamPrimGt4PreXformEnvLayer`.
/// `objectFlags` is ignored, including transparency and reverse-culling bits.
/// `elements` starts after the three-word record header. The caller supplies
/// `workspace->elemCount` (0..65535) and `elemStride` in u32 words. The first
/// four u16 values of each element are depth-cache byte references: clear
/// bits 0..1 and divide by four to select `szTable` entries. Those low bits'
/// role is unproven. Each index must be below 1024 and initialized by an earlier
/// projection in this draw walk. Drawing reads eight payload bytes; construction
/// reads through word 4, requiring at least five words, not a proven full extent.
///
/// `preXformWrite` supplies two writable, word-aligned `POLY_GT4` slots per
/// element in the selected buffer half's first region: blended layer first,
/// opaque base second. Earlier commands supply signed screen XY in pixels
/// and lit colours. Construction writes both packets' U/V from the element,
/// adds the object's layer page and CLUT offsets to the first packet and the
/// model's page and CLUT offsets to the second, and sets ABR bit 5 on the
/// layer page. This handler preserves those colours, coordinates, U/V, pages
/// and CLUTs, and it does not fold any corner onto another.
///
/// Positive NCLIP area of corners 0/1/2 accepts the element immediately.
/// Otherwise corners 1/2/3 are tested, and only a negative area accepts.
/// Zero area rejects the half just tested. Any `TMD_VERTEX_DEPTH_INVALID`
/// corner rejects both packets.
///
/// Loads screen Z into SZ0..SZ3 and runs AVSZ4 with the current ZSF4 scale.
/// Accepted packets receive twelve-word DMA lengths and command bytes `0x3E`
/// (blended layer) and `0x3C` (opaque base). OTZ is stored after the layer
/// command and stored again after the base command; both writes are the same
/// average. Unsigned OTZ is shifted by `gDisplayState.otDepthShift`, divided
/// by 16 and wrapped to 0..1023 relative to `workspace->ot`, already displaced
/// by the object's signed tag offset. The selected OT must contain that entry.
/// Links the layer then the base, so head insertion draws the opaque base
/// before the blended layer.
///
/// Advances `preXformWrite` by one 104-byte pair per element even when culled,
/// returns `elements + initial elemCount * elemStride`, and leaves `elemCount`
/// at -1 even for an empty record. Reuses `gteResult` for facing and OTZ.
/// Workspace, stream and depth cache are borrowed for the draw walk; no pointer
/// is retained here. Linked packets and OT must live until GPU consumption ends.
u32* tmdDrawStreamPrimGt4PreXformOffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

#endif // GAMEPLAY_MODEL_OBJECTS_H
