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

TmdObject* Gp_AttachTmd(Task* task, TmdSource* src);

/// Gives a task a coordinate body and returns it, or `NULL` when there is no
/// memory for one, in which case the task is left without a body.
///
/// The body carries a coordinate of its own instead of a model. That coordinate
/// is parented to the view, so what the task places in it comes back relative to
/// the camera rather than to the world, and it joins the end of
/// `gModelObjectCoordBodyList`, where the frame's draw passes compose it.
/// Recording it as the task's body (`bodyKind` `TASK_BODY_COORD`) is what later
/// releases it; `Gp_AttachTmd` is the model-side counterpart.
ModelObjectCoordBody* gpAttachDisp2d(Task* task);

TmdObject* Gp_AttachTmdFlags(Task* task, TmdSource* src, s32 flags);

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
/// `model` is the live object `Tmd_Create` returned, including one attached by
/// `Gp_AttachTmd` or `Gp_AttachTmdFlags`. It is not `NULL`. That address is the
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
/// `body` must be `NULL` or the original live allocation from `gpAttachDisp2d`.
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

/// Draw-pass handler of a stream's layered pre-transformed textured-quad records
/// (`0x4079`): each element's two quads — the base the model is drawn from and the
/// semi-transparent layer drawn over it — are culled against the corners the
/// packets already carry, stamped, and linked into the ordering table at the depth
/// those corners average to.
///
/// The opcode says the quads' corners are already in screen space: the record's
/// vertex commands put them there and left each corner's screen Z in the
/// per-vertex cache, where a corner whose projection failed carries its sign bit.
/// The element's corner refs index that cache, and an element naming a failed
/// corner is passed over. The facing test is taken on the layer's corners, read
/// back out of the packet, as the two triangles the quad's corners make: where one
/// half alone survives, the element is trimmed onto it — the corner that half does
/// not use takes a copy of one it does, in each of the element's two packets — and
/// where neither survives the element is passed over. Either way its packets' room
/// is stepped over, so the primitives stay in step with the elements that named
/// them.
///
/// The layer is this pass's to texture. Its page is the one the corners' marks
/// select — the marks being what the record's vertex commands left in the layer's
/// corner code bytes as they worked out its texture coordinates — and a corner
/// carrying none has its coordinate folded back into that page, dropped by `0x80`
/// where it ran into the upper half of its range and taken as the page's start
/// where it did not. Both packets are then stamped with their length and their
/// code, `0x3E` on the layer and `0x3C` on the base, and filed together at the
/// depth the four cached corners average to.
///
/// The record has two draw handlers, and this is the one that runs where the layer
/// is left for this pass to texture. The other runs where the process pass textured
/// the layer itself, page and coordinates both coming from the object's offsets
/// (`gpStreamPrimGt4PreXformOffsetLayer` is that record's process-pass handler).
/// Which of the two a record gets is settled where its handler is resolved, from
/// the area the session is in, so the `flags` this one is handed goes unread.
u32* gpDrawStreamPrimGt4PreXformLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Draw handler of a stream's layered pre-transformed textured-triangle records
/// (`0x4039`) whose semi-transparent layer is textured from the drawing object:
/// each element's two packets are completed and linked into the ordering table at
/// the depth its three corners average to.
///
/// The element is the plain pre-transformed triangle's
/// (`tmdDrawStreamPrimGt3PreXform`), with the layered draw's second packet: its refs
/// name the triangle's three corners in the per-vertex screen-Z cache rather than in
/// the vertex array, because the stream's transform commands have already written
/// each corner's screen coordinates and lit colour into both packets, and its depth
/// into that cache. What a frame settles is what a transform cannot: the facing, out
/// of the corners already in the packets; the slot the corners' average depth files
/// the pair under; the length and primitive code both packets are drawn with; and
/// their two links. A corner whose cached depth carries the transform's error mark,
/// or a triangle that turns away, leaves both packets out of the ordering table,
/// though their room is stepped over either way, so the primitives stay in step with
/// the elements that named them.
///
/// The two are one layered draw — the base the model is drawn from, and the
/// semi-transparent layer blended over it — which is the difference between the two
/// codes they are stamped with: `0x34` for the base and `0x36` for the layer, the
/// semi-transparency bit between them. Nothing of the layer's texture is the draw
/// pass's to settle here: it takes its page and CLUT from the drawing object's
/// offsets, which the pass that builds the primitives wrote in. The record's other
/// entry is the same walk with the layer's page settled there instead, and the
/// session's current place is what picks between them; neither entry reads `flags`.
u32* gpDrawStreamPrimGt3PreXformOffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// The draw pass's handler for a stream's layered pre-transformed textured-quad
/// records (`0x4079`) whose semi-transparent layer is textured from the object:
/// each element's two quads sit in the buffer half's first region — the quad the
/// model is drawn from and the layer drawn over it — and this handler links both
/// into the ordering table at the depth their corners measured.
///
/// Nothing is projected or lit here. The record is pre-transformed, so a
/// transform command has already written the corners and lit colours into the
/// packets and their depths into the per-vertex screen-Z table, where an element
/// names the four corners of its quad. What a frame adds is the filing: those
/// four cached depths are averaged for the ordering-table link, the facing comes
/// from the coordinates the packets already carry, and each packet is given the
/// length and primitive code it is drawn under — `0x3E` for the layer and `0x3C`
/// for the base, the two linked back to back so that the base is drawn under the
/// layer. A quad is drawn where either of its halves faces the viewer, and left
/// out of the ordering table where neither does; the room its packets take is
/// passed over either way, which keeps the packets in step with the elements that
/// named them.
///
/// The layer's texture is not this entry's to settle: the command that builds the
/// record writes the page and CLUT the layer draws from — the object's own page
/// and CLUT offsets — and the coordinates it samples, so nothing here touches
/// either packet's texture words. That is what distinguishes this entry from the
/// record's other handler, which is taken where the layer is the drawing pass's
/// to texture. The record has no variant for `flags` to select, so it goes
/// unread.
u32* gpDrawStreamPrimGt4PreXformOffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream);

#endif // GAMEPLAY_MODEL_OBJECTS_H
