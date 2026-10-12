// Runs the asset map's script without a browser, under a stub document:
//   node tools/peassets/asset_map_smoke.js local/asset_map.html o.gameplay o.shelter_b3_dumping_hole a.pe2img_282
// Each argument is a subject as the page's address names it: o.<overlay>, a.<asset>, d.<stage>/<folder>, f.<file index>, n. (the proposed names) or
// t.<trace>/<session number> (a trace session's playback; t. lists the traces). It prints the subject's
// sections with their tiles and what is flagged. It catches a script that throws and lets the content be read as
// text; it does not check layout. In a trace's playback it presses the room, view and load jumps in both directions,
// by button and by key, and fails when a room jump does not land in that room with its first view resident.
const fs = require("fs"), path = require("path");
const page = process.argv[2], html = fs.readFileSync(page, "utf8"), js = html.slice(html.indexOf("<script>") + 8, html.lastIndexOf("</script>"));
const els = new Map();
function node(tag) {
  const n = { tag, children: [], attrs: {}, style: { setProperty() {} }, classList: { toggle() {}, add() {}, remove() {} }, hidden: false, value: "", className: "", checked: true, _text: "",
    setAttribute(k, v) { this.attrs[k] = v; if (k === "value" && this.tag !== "option") this.value = v; }, getAttribute(k) { return this.attrs[k]; }, addEventListener(type, f) { (this.on = this.on || {})[type] = f; }, scrollTo() {},
    append(...c) { for (const x of c) { this.children.push(x); if (x && x.tag === "script" && x.src) { try { eval(fs.readFileSync(path.join(path.dirname(page), x.src), "utf8")); } catch (e) { x.onerror && x.onerror(); } } } },
    replaceChildren(...c) { this.children = c; }, querySelectorAll() { return []; }, remove() {},
    getContext() { return { fillRect() {}, strokeRect() {}, clearRect() {}, save() {}, restore() {}, clip() {}, arc() {}, setTransform() {}, drawImage() {}, beginPath() {}, moveTo() {}, lineTo() {}, closePath() {}, fill() {}, stroke() {}, createImageData: (w, h) => ({ data: new Uint8ClampedArray(w * h * 4) }), getImageData: (x, y, w, h) => ({ data: w > 1 ? new Uint8ClampedArray(w * h * 4).fill(255) : [200, 0, 0, 255] }), putImageData() {} }; } };
  Object.defineProperty(n, "textContent", { get() { return this._text + this.children.map(c => typeof c === "string" ? c : c && c.textContent || "").join(""); }, set(v) { this._text = String(v); this.children = []; } });
  Object.defineProperty(n, "lastChild", { get() { return this.children[this.children.length - 1]; } });
  return n;
}
global.document = { getElementById: id => els.get(id) || (els.set(id, node("#" + id)), els.get(id)), createElementNS: (_, t) => node(t), createElement: t => node(t), head: node("head"), documentElement: node("html"), querySelector: () => node("main") };
global.localStorage = { getItem: () => null, setItem() {} }; global.window = global; global.location = { hash: "" }; const listeners = {}; global.addEventListener = (type, f) => { (listeners[type] = listeners[type] || []).push(f); }; global.getComputedStyle = () => ({ getPropertyValue: () => "#f00" }); global.atob = s => Buffer.from(s, "base64").toString("binary");
const walk = (n, f) => { if (!n || typeof n === "string") return; f(n); (n.children || []).forEach(c => walk(c, f)); };
eval(js + `
;(async () => {
  for (const subject of process.argv.slice(3)) {
    const [kind, ...rest] = subject.split("."), name = rest.join(".");
    await (kind === "t" ? showTrace(name) : kind === "a" ? showAsset(name) : kind === "f" || kind === "d" ? showFiles(name) : kind === "n" ? showNames() : kind === "g" ? showGroups(name) : showOverlay(name));
    const out = [];
    for (const s of document.getElementById("subject").children) { if (s.tag !== "section") continue; let tiles = 0, flagged = 0, rows = 0, shots = 0; const flags = [];
      walk(s, n => { if (/^tile( |$)/.test(n.className)) { tiles++; if (/miss/.test(n.className)) { flagged++; flags.push(n.textContent.slice(0, 150)); } } if (n.tag === "tr" || n.tag === "li") rows++; if (n.className === "shot") shots++; });
      out.push("  " + s.children[0].textContent + (tiles ? ": " + tiles + " tiles, " + flagged + " flagged" : "") + (shots ? ": " + shots + " shown" : "") + (rows ? ": " + rows + " rows" : "")); flags.slice(0, 3).forEach(f => out.push("      " + f)); }
    console.log(subject + "\\n" + out.join("\\n"));
  }
  // Every session of every trace is opened and stepped: through each load and change (the load jump's pass below), then record by record at the
  // start, the end and back. It says what the playback holds at the last record; a script that throws fails the run.
  for (const [name, sessions] of D.traces || []) for (const s of sessions) {
    await showTrace(name + "/" + s[1]); const T = window.__trace; let stops = 0, at = T.pos();
    const picked = {}, look = () => walk(document.getElementById("subject"), n => { if (n.tag === "select" && /^rs-/.test(n.attrs.id || "")) picked[n.attrs.id] = n.value; });
    for (let k = 0; k < 6; k++) await T.go(T.pos() - 1); for (let k = 0; k < 3; k++) await T.go(T.lo + k); await T.go(T.hi);
    // The jumps. Each level's buttons are pressed on from the first record until the button is disabled, then back
    // from the last (the view and load levels 40 times, and then at their first stop): every press must land on that level's next stop, and the line beside the buttons must have named it.
    const find = id => { let hit = null; walk(document.getElementById("subject"), n => { if (!hit && n.attrs && n.attrs.id === id) hit = n; }); if (!hit) throw new Error("no element " + id + " in the playback"); return hit; };
    const press = async b => { if (!b.disabled) await b.on.click({}); }, keydown = async (key, shiftKey, target) => { for (const f of listeners.keydown || []) await f({ key, shiftKey, target: target || { tagName: "BODY" }, preventDefault() {} }); };
    const residentAt = n => T.trace.things.filter(x => x[3] <= n && (x[4] < 0 || x[4] > n)), placeAt = n => T.trace.places[upTo(T.trace.places, n) - 1];
    const landed = (level, x) => { if (T.pos() !== x[0]) throw new Error(level + " jump: at record " + T.pos() + ", expected " + x[0]);
      if (level !== "room") return; const p = placeAt(x[0]), here = residentAt(x[0]);
      if (!p || p[7] !== x[1]) throw new Error("room jump to record " + x[0] + ": the location there is " + (p && p[7]) + ", not " + x[1]);
      if (!here.some(q => q[0] === p[9] || x[7].includes(q[2]))) throw new Error("room jump to record " + x[0] + " (" + x[1] + "): neither the background nor anything of its first view's loads is resident"); };
    const pressed = {};
    for (const level of ["room", "view", "load"]) { const on = find("tr-" + level + "-on"), back = find("tr-" + level + "-back"), said = find("tr-to-" + level), list = level === "load" ? null : T.stops[level]; let n = 0;
      await T.go(T.lo); if (!back.disabled) throw new Error(level + ": the back button is live at the session's first record");
      for (let i = 0; !on.disabled; i++) { const before = T.pos(), want = list ? list.find(x => x[0] > before) : [T.big(1)];
        if (list && !said.textContent.includes(want[1] + " · variant " + want[2] + " · view " + want[3]) ) throw new Error(level + ": the line does not name the next stop: " + said.textContent);
        if (!list && !said.textContent.includes("record " + want[0] + ":")) throw new Error("load: the line does not name the next stop: " + said.textContent);
        await press(on); landed(level, want); n++; if (!list) { stops++; look(); } if (i > T.hi - T.lo) throw new Error(level + " forward never ends"); }
      const end = T.pos(); await on.on.click({}); if (T.pos() !== end) throw new Error(level + " forward from the last stop moved to " + T.pos());
      if (list && n !== list.filter(x => x[0] > T.lo).length) throw new Error(level + ": " + n + " stops pressed through, the session has " + list.length);
      await T.go(T.hi); if (!on.disabled) throw new Error(level + ": the forward button is live at the session's last record"); let m = 0; for (; !back.disabled && (level === "room" || m < 40); m++) { const before = T.pos(), want = list ? list.filter(x => x[0] < before).pop() : [T.big(-1)]; await press(back); landed(level, want); if (m > T.hi - T.lo) throw new Error(level + " back never ends"); }
      // Going back recomposes video memory, so the view and load levels are pressed back 40 times and then from just past their first stop.
      await T.go(T.lo); if (!on.disabled) { await press(on); const first = T.pos(); if (first < T.hi) { await T.go(first + 1); await press(back); if (T.pos() !== first) throw new Error(level + " back to the first stop: at record " + T.pos() + ", expected " + first); m++; } while (!back.disabled) await press(back); }
      const start = T.pos(); await back.on.click({}); if (T.pos() !== start) throw new Error(level + " back from the first stop moved to " + T.pos());
      pressed[level] = n + " on, " + m + " back"; }
    // The keys: the arrows a record, with Shift a view, [ ] a room, , . a load; none while a text field has the focus.
    const R = T.stops.room, V = T.stops.view; await T.go(T.lo); let keys = 0;
    const expect = (what, n) => { if (T.pos() !== n) throw new Error("key " + what + ": at record " + T.pos() + ", expected " + n); keys++; };
    await keydown("ArrowRight"); expect("right", Math.min(T.hi, T.lo + 1)); await keydown("ArrowLeft"); expect("left", T.lo); await keydown("ArrowLeft"); expect("left at the first record", T.lo);
    await keydown("ArrowRight", false, { tagName: "INPUT", type: "text" }); expect("right in a text field", T.lo); await keydown("]", false, { tagName: "SELECT" }); expect("] in a list", T.lo);
    await keydown("ArrowRight", false, { tagName: "INPUT", type: "range" }); expect("right on the slider (the slider's own)", T.lo);
    if (V.length) { await keydown("ArrowRight", true); expect("Shift+right", V[0][0]); if (V[1]) { await keydown("ArrowRight", true, { tagName: "INPUT", type: "range" }); expect("Shift+right on the slider", V[1][0]); await keydown("ArrowLeft", true); expect("Shift+left", V[0][0]); } }
    if (R.length) { await T.go(T.lo); await keydown("]"); expect("]", R[0][0]); landed("room", R[0]); if (R[1]) { await keydown("]"); expect("]", R[1][0]); await keydown("["); expect("[", R[0][0]); } await keydown("["); expect("[ at the first room", R[0][0]); }
    await T.go(T.lo); const firstLoad = T.big(1); await keydown("."); expect(".", firstLoad); const loadBack = T.big(-1); await keydown(","); expect(",", loadBack);
    // The notes: a list of the session's, each a link that moves the playback to its record, where the record list marks it as a note.
    const notes = find("tr-notes"), items = notes.children.filter(c => c && c.tag === "li"); if (items.length !== T.stops.note.length) throw new Error("notes: " + items.length + " listed, the session has " + T.stops.note.length);
    for (let i = 0; i < items.length; i++) { const a = items[i].children.find(c => c && c.tag === "a"); await a.on.click({}); if (T.pos() !== T.stops.note[i]) throw new Error("note link " + i + " went to record " + T.pos());
      let marked = false; walk(document.getElementById("subject"), n => { if (/k-note/.test(n.className) && / cur/.test(n.className)) marked = true; }); if (!marked) throw new Error("note at record " + T.pos() + " is not marked in the record list"); }
    console.log("trace " + name + " session " + s[1] + " jumps: room " + pressed.room + " (" + R.length + " rooms, " + R.filter(x => x[6]).length + " without a room start and view ready), view " + pressed.view + " (" + V.length + " views), load " + pressed.load + "; " + keys + " key presses checked; " + items.length + " notes listed and followed");
    await T.go(T.hi);
    const live = T.trace.things.filter(x => x[3] <= T.hi && (x[4] < 0 || x[4] > T.hi)), count = m => live.filter(x => x[1] === m).length; let secs = 0, rows = 0, rects = 0;
    walk(document.getElementById("subject"), n => { if (n.tag === "section") secs++; if (n.tag === "tr") rows++; if (n.tag === "rect") rects++; });
    // The page's video memory against the replay's list: a resident rectangle was landed on by nothing since, so every word of it must still be its asset's.
    const mem = traceMemory(T.trace, upTo(T.trace.ops, T.hi)); let whole = 0, broken = [];
    for (const x of live) if (x[1] === "v") for (const r of x[6]) { let ok = true; for (let y = r[1]; y < r[1] + r[3] && ok; y++) for (let xx = r[0]; xx < r[0] + r[2]; xx++) if (mem.names[mem.owner[y * 1024 + xx]] !== x[0]) { ok = false; break; } ok ? whole++ : broken.push(x[0]); }
    console.log("trace " + name + " session " + s[1] + " " + s[0] + (s[3] ? " (dropped)" : "") + ": records " + T.lo + "-" + T.hi + ", " + stops + " stops stepped; at the end " + count("v") + " in video memory, " + count("m") + " in main memory, " + count("s") + " in sound memory; " + secs + " sections, " + rows + " rows, " + rects + " outlines; room viewer " + (picked["rs-room"] === undefined ? "never drawn" : "last drawn at room option " + picked["rs-room"] + ", variant " + picked["rs-var"] + ", view " + (picked["rs-view"] || "free")) + "; " + whole + " resident rectangles whole in the composed memory" + (broken.length ? ", NOT whole: " + broken.join(", ") : ""));
  }
  if (!(D.traces || []).length) { await showTrace(""); console.log("no traces beside this page: " + document.getElementById("subject").textContent.slice(0, 80)); }
  navMode = "t"; drawNav(); let tl = 0; walk(document.getElementById("list"), n => { if (n.tag === "a") tl++; }); console.log("navigator: " + tl + " trace links");
  navMode = "o"; drawNav(); let links = 0; walk(document.getElementById("list"), n => { if (n.tag === "a") links++; }); console.log("navigator: " + links + " overlay links");
  navMode = "f"; drawNav(); links = 0; walk(document.getElementById("list"), n => { if (n.tag === "a") links++; }); console.log("navigator: " + links + " file links");
  navMode = "a"; drawNav(); links = 0; walk(document.getElementById("list"), n => { if (n.tag === "a") links++; }); console.log("navigator: " + links + " asset links");
})().catch(e => { console.error("FAILED", e); process.exit(1); });`);
