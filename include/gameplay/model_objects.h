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

/// Draw handler of a stream's pre-transformed flat-quad records (`0x45`): each
/// element contributes one untextured quad whose corners are already in screen
/// space, and links its packet into the ordering table at the depth those corners
/// measured.
///
/// Nothing is projected or lit here. A transform record has already written the
/// element's projected corners into the packet this command files, and their
/// depths into the per-vertex screen-Z cache, which is why the element names its
/// corners in that cache rather than in the vertex array. What is left is what a
/// transform cannot settle: whether the quad survives its facing tests — one per
/// half the diagonal cuts it into, both taken from the coordinates the packet
/// already carries — which ordering-table slot the corners' average depth puts it
/// in, and the packet's link word. An element whose cached depth carries the
/// transform's error mark, or whose quad the facing tests reject, is passed over
/// — its packet slot is stepped over either way, which is what keeps the packets
/// in step with the elements that named them.
///
/// The record's other half is the build pass's command (`modelLightingStreamPrimF4PreXform`),
/// which laid the packet out and gave it its length, its primitive code and the
/// element's colour; this command writes none of the three.
u32* gpDrawStreamPrimF4PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Draw-pass handler of a stream's pre-transformed flat-triangle records
/// (`0x5`): each element contributes one untextured `POLY_F3` to the buffer
/// half's first region, where its corners are already in screen space, and links
/// it into the ordering table at the depth its three corners average to.
///
/// Nothing here transforms or lights the triangle: the pass that projects the
/// stream's vertices (`tmdXformStreamVerts`) has already written each corner's
/// screen coordinates into the packet this handler files, and its depth into the
/// per-vertex screen-Z table, so an element names its three corners in that table
/// rather than in the vertex array. The packet's fixed fields — its length, its
/// primitive code and the element's colour — are the build pass's
/// (`modelLightingStreamPrimF3PreXform`), so what a frame adds is the triangle's filing:
/// the three cached depths are averaged for the ordering-table link, and the
/// facing comes from the coordinates the packet already carries.
///
/// An element with any corner depth marked `TMD_VERTEX_DEPTH_INVALID` by the
/// projection pre-pass, or whose triangle turns away, is stepped over rather
/// than linked. The record has no variant for `flags` to select, so it goes unread.
u32* gpDrawStreamPrimF3PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// The draw pass's handler for a stream's layered pre-transformed textured-triangle
/// records whose semi-transparent layer is textured from a page of its own
/// (`0x4039`): each element's two triangles are filed and linked into the ordering
/// table.
///
/// The record is the pre-transformed `0x39` triangle's with `0x4000` set, so its
/// corners are already in screen space — the vertex commands have written each
/// corner's coordinates and lit colour into the packets and its depth into the
/// per-vertex screen-Z table — and each element contributes the two packets built
/// for it: the semi-transparent layer, and the base the model is drawn from. The
/// element names its three corners in that table, so both packets are culled and
/// filed from the one triangle it names. Nothing is projected or lit here; what a
/// frame adds is the pair's filing: the three cached depths are averaged for the
/// ordering-table links, the facing comes from the coordinates the packets already
/// carry, and each packet's length and primitive code are written — the layer's
/// the blended `0x36`, the base's the opaque `0x34`. An element whose cached depth
/// carries the transform's error mark, or whose triangle turns away, is passed
/// over, though both packets' room is stepped over either way, so the primitives
/// stay in step with the elements that named them.
///
/// The layer's page is settled here as well, because the record's layer is
/// textured from a page of its own rather than from the model's. The command that
/// lays the packets out (`gpStreamPrimGt3PreXformFixedLayer`) fixes the layer's
/// page and leaves its texture coordinates to the vertex commands, which derive
/// them from the vertices' normals; this handler picks between the two pages the
/// layer may be drawn from, and takes with it the corners whose texture
/// coordinates are not already on the page it picks. Where the layer takes the
/// object's own page and CLUT offsets instead, the command has written them and
/// the walk takes the record's other entry, which leaves the page alone. The entry
/// is picked when the model's stream is resolved, by the area the session is in, so
/// `flags` selects nothing here and goes unread.
u32* gpDrawStreamPrimGt3PreXformFixedLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
