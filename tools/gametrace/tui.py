#!/usr/bin/env python3
"""A terminal front end for gametrace.py.

    tui.py [--bind] [any arguments of `gametrace.py run`]     e.g.  tui.py     tui.py --bind --no-log

It runs the tracer as a program of its own (`gametrace.py run --machine ...`), shows what the tracer says, and
sends it what is typed. It starts unbound - no tracer running, nothing connected to the game - and the tracer is
started from inside with `bind`; `--bind` starts it at once. `bind` alone uses the arguments this program was
given; `bind --no-log --session boot` gives its own. `unbind` detaches the tracer (as its `quit` does) and
leaves this program open, to bind again. The tracer does the tracing and knows nothing of this file: it only reads lines and
writes lines, and can always be run bare. This file needs Textual (see requirements.txt); the tracer needs
nothing.

What is on the screen:

    the top line     what the game is doing (running, paused, ...), where the player is, what is being watched
    the middle       the watched events as they happen, the tracer's messages, and the answers to commands
    the bottom line  where commands are typed: `bind` and `unbind`, and any command of the tracer (`help` lists them)

A command of the tracer that asks a question shows its prompt above the input line; one that offers a list opens
it as a list to pick from (type to narrow it, Enter to take, Escape to give up).

Ctrl+P opens the command palette, which lists every command there is - this program's and the tracer's, those a
hooks file adds included, since the tracer says which it has. Picking one that needs no more carries it out;
one that needs an argument is put on the input line to be finished.

Keys:  Up/Down  earlier commands     F2  pause or resume the game     F3  what is being watched
       Ctrl+L   clear the middle     Ctrl+Q  detach the tracer and leave (the game is left as `quit` leaves it)
"""
from __future__ import annotations

import asyncio
import json
import sys
from pathlib import Path

from rich.markup import escape
from rich.text import Text
from textual import events
from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.command import DiscoveryHit, Hit, Provider
from textual.containers import Vertical
from textual.screen import ModalScreen
from textual.widgets import Input, OptionList, RichLog, Static
from textual.widgets.option_list import Option

TRACER = Path(__file__).with_name("gametrace.py")
STATE_STYLE = {"running": "green", "paused": "yellow", "waiting": "cyan", "detached": "red", "starting": "cyan", "unbound": "white"}


class CommandLine(Input):
    """The input line, with the earlier commands on Up and Down."""

    def __init__(self, **kwargs) -> None:
        super().__init__(**kwargs)
        self.history: list[str] = []
        self.at = 0                                    # how far back in the history the line now shown is (0: a new line)
        self.draft = ""

    def remember(self, line: str) -> None:
        if line and (not self.history or self.history[-1] != line):
            self.history.append(line)
        self.at = 0

    def _on_key(self, event: events.Key) -> None:
        if event.key in ("up", "down") and self.history:
            if self.at == 0:
                self.draft = self.value
            self.at = max(0, min(len(self.history), self.at + (1 if event.key == "up" else -1)))
            self.value = self.history[-self.at] if self.at else self.draft
            self.cursor_position = len(self.value)
            event.stop()
            event.prevent_default()


class Chooser(ModalScreen):
    """A list a command of the tracer offers. The result is the name picked, or "" for giving up."""

    BINDINGS = [Binding("escape", "give_up", "give up")]

    def __init__(self, title: str, options: list[list[str]]) -> None:
        super().__init__()
        self.title_text, self.options = title, options

    def compose(self) -> ComposeResult:
        with Vertical(id="chooser"):
            yield Static(escape(self.title_text), id="chooser-title")
            yield Input(placeholder="type to narrow the list; Enter takes the one marked", id="chooser-filter")
            yield OptionList(id="chooser-list")

    def on_mount(self) -> None:
        self.fill("")
        self.query_one("#chooser-filter", Input).focus()

    def shown(self, text: str) -> list[list[str]]:
        text = text.lower()
        return [o for o in self.options if text in o[0].lower() or text in o[1].lower()]

    def fill(self, text: str) -> None:
        box = self.query_one("#chooser-list", OptionList)
        box.clear_options()
        box.add_options([Option(Text.assemble(name, ("   " + about if about else "", "dim")), id=str(n)) for n, (name, about) in enumerate(self.shown(text))])
        if box.option_count:
            box.highlighted = 0

    def on_input_changed(self, event: Input.Changed) -> None:
        self.fill(event.value)

    def on_input_submitted(self, event: Input.Submitted) -> None:            # Enter in the filter line takes the marked option
        box, listed = self.query_one("#chooser-list", OptionList), self.shown(event.value)
        if listed:
            self.dismiss(listed[box.highlighted or 0][0])

    def on_option_list_option_selected(self, event: OptionList.OptionSelected) -> None:
        self.dismiss(self.shown(self.query_one("#chooser-filter", Input).value)[event.option_index][0])

    def _on_key(self, event: events.Key) -> None:                             # Up and Down move the mark while the filter line has the keys
        if event.key in ("up", "down"):
            box = self.query_one("#chooser-list", OptionList)
            if box.option_count:
                box.highlighted = max(0, min(box.option_count - 1, (box.highlighted or 0) + (1 if event.key == "down" else -1)))
            event.stop()
            event.prevent_default()

    def action_give_up(self) -> None:
        self.dismiss("")


class TracerCommands(Provider):
    """The command palette's entries for this program's own commands and for the tracer's, which the tracer
    itself says when it starts - so a command added in a hooks file is here without anything being changed."""

    def entries(self) -> list[tuple[str, str, str]]:
        app = self.app
        own = [("unbind", "", "detach the tracer and stay here")] if app.bound else [("bind", "[ARGUMENTS]", "start the tracer")]
        return own + [(c["name"], c.get("arguments", ""), c.get("about", "")) for c in app.tracer_commands if app.bound]

    async def discover(self):
        for name, arguments, about in self.entries():
            yield DiscoveryHit(f"{name} {arguments}".strip(), lambda n=name, a=arguments: self.app.pick_command(n, a), help=about)

    async def search(self, query: str):
        matcher = self.matcher(query)
        for name, arguments, about in self.entries():
            shown = f"{name} {arguments}".strip()
            score = max(matcher.match(shown), matcher.match(about) * 0.8)
            if score > 0:
                yield Hit(score, matcher.highlight(shown), lambda n=name, a=arguments: self.app.pick_command(n, a), help=about)


class TraceApp(App):
    COMMANDS = App.COMMANDS | {TracerCommands}
    CSS = """
    #status { height: 1; background: $panel; color: $text; padding: 0 1; }
    #log { height: 1fr; border: none; padding: 0 1; }
    #repeat { height: 1; color: $text-muted; padding: 0 1; }
    #prompt { height: auto; max-height: 3; color: $accent; padding: 0 1; display: none; }
    #prompt.asking { display: block; }
    #line { border: tall $panel; height: 3; }
    Chooser { align: center middle; }
    #chooser { width: 80%; max-width: 110; height: 80%; border: thick $accent; background: $surface; padding: 1 2; }
    #chooser-title { height: auto; margin-bottom: 1; text-style: bold; }
    #chooser-list { height: 1fr; margin-top: 1; }
    """
    BINDINGS = [
        Binding("f2", "pause", "pause/resume"),
        Binding("f3", "watching", "watching"),
        Binding("ctrl+l", "clear", "clear"),
        Binding("ctrl+q", "leave", "detach and leave", priority=True),
    ]

    def __init__(self, tracer: list[str], arguments: list[str] | None = None, bind: bool = False) -> None:
        super().__init__()
        self.command = tracer                          # the program to run: the tracer, or a stand-in for a test
        self.arguments = arguments or []               # what `bind` gives it when it is typed alone
        self.bind_at_start = bind
        self.process: asyncio.subprocess.Process | None = None
        self.state = "unbound"
        self.place: dict = {}                          # where the player is, from the events that say
        self.room = ""
        self.watched: list[str] = []
        self.repeats = 0
        self.asking = False
        self.tracer_commands: list[dict] = []          # the commands the tracer says it has
        self.said: list[str] = []                      # what has been shown in the middle, as plain text (for tests)

    def compose(self) -> ComposeResult:
        yield Static("", id="status")
        yield RichLog(id="log", wrap=True, highlight=False, markup=True, max_lines=5000)
        yield Static("", id="repeat")
        yield Static("", id="prompt")
        yield CommandLine(placeholder="a command of the tracer; `help` lists them", id="line")

    async def on_mount(self) -> None:
        self.title = "gametrace"
        self.show_status()
        self.query_one("#line", CommandLine).focus()
        if self.bind_at_start:
            self.bind(self.arguments)
        else:
            self.show("[dim]not bound: no tracer is running. `bind` starts it" + (f" (with {escape(' '.join(self.arguments))})" if self.arguments else "") + "; `bind ARGUMENTS` gives it its own.[/dim]")

    # ---- the tracer

    @property
    def bound(self) -> bool:
        return self.process is not None and self.process.returncode is None

    def bind(self, arguments: list[str]) -> None:
        """Start the tracer, with these arguments after its own."""
        if self.bound:
            self.show("[red]already bound: `unbind` first[/red]")
            return
        self.place, self.room, self.watched, self.asking = {}, "", [], False
        self.set_state("starting")
        self.run_worker(self.follow([*self.command, *arguments]), exclusive=True, name="tracer")

    async def follow(self, command: list[str]) -> None:
        """Run the tracer and take in everything it says, a line at a time, until it ends."""
        try:
            self.process = await asyncio.create_subprocess_exec(*command, stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE,
                                                                stderr=asyncio.subprocess.PIPE, limit=1 << 22)
        except OSError as error:
            self.show(f"[red]the tracer could not be started: {escape(str(error))}[/red]")
            self.set_state("unbound")
            return
        errors = asyncio.create_task(self.follow_errors())
        while True:
            raw = await self.process.stdout.readline()
            if not raw:
                break
            text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            try:
                said = json.loads(text)
            except ValueError:
                said = None
            if isinstance(said, dict) and "kind" in said:
                self.take(said)
            elif text:
                self.show(escape(text))                # not one of the tracer's lines: shown as it came
        await errors
        code = await self.process.wait()
        self.query_one("#prompt", Static).remove_class("asking")
        self.asking = False
        self.end_repeat()
        self.set_state("unbound")
        self.show(f"[dim]the tracer has ended{'' if code == 0 else f' (exit code {code})'}: not bound. `bind` starts it again; Ctrl+Q leaves.[/dim]")

    async def follow_errors(self) -> None:
        while True:
            raw = await self.process.stderr.readline()
            if not raw:
                return
            self.show(f"[red]{escape(raw.decode('utf-8', errors='replace').rstrip())}[/red]")

    def send(self, line: str) -> None:
        if not self.bound or self.process.stdin is None:
            self.show("[red]not bound: `bind` starts the tracer[/red]")
            return
        self.process.stdin.write((line + "\n").encode("utf-8"))

    def take(self, said: dict) -> None:
        """One thing the tracer said (see `tell` in gametrace.py)."""
        kind, text = said["kind"], said.get("text", "")
        if kind == "event":
            self.learn(said.get("record") or {})
            if said.get("watched"):
                if said.get("repeats"):
                    self.repeats = said["repeats"]
                    self.query_one("#repeat", Static).update(f"  the line above has come {self.repeats + 1} times")
                else:
                    self.end_repeat()
                    name, _, rest = text.partition("  ")
                    self.show(f"[bold cyan]{escape(name)}[/bold cyan]  {escape(rest)}")
        elif kind == "state":
            self.set_state(said.get("state", text))
        elif kind == "commands":
            self.tracer_commands = said.get("commands", [])
        elif kind == "ask":
            self.asking = True
            prompt = self.query_one("#prompt", Static)
            prompt.update(escape(text))
            prompt.add_class("asking")
        elif kind == "choose":
            self.push_screen(Chooser(said.get("title", text), said.get("options", [])), self.send)
        elif kind == "reply":
            self.end_repeat()
            self.show(f"[green]{escape(text)}[/green]")
        else:
            self.end_repeat()
            self.show(f"[dim]{escape(text)}[/dim]")

    def learn(self, record: dict) -> None:
        """Keep the top line current from what the events say: where the player is, which room's files load."""
        event = record.get("event")
        if event in ("view_load", "view_ready", "room_start", "variant_set") and "stage" in record:
            self.place = {k: record.get(k) for k in ("stage", "area", "room", "view", "variant")}
        if event == "sound_reset_for_area" and record.get("room"):
            self.room = record["room"]
        elif event == "file_load" and record.get("folder_name"):
            self.room = record["folder_name"]
        if event in ("view_load", "view_ready", "room_start", "variant_set", "sound_reset_for_area", "file_load"):
            self.show_status()

    # ---- the screen

    def show(self, markup: str) -> None:
        self.said.append(Text.from_markup(markup).plain)
        self.query_one("#log", RichLog).write(markup)

    def end_repeat(self) -> None:
        if self.repeats:
            self.repeats = 0
            self.query_one("#repeat", Static).update("")

    def set_state(self, state: str) -> None:
        self.state = state
        self.show_status()

    def show_status(self) -> None:
        colour = STATE_STYLE.get(self.state, "white")
        parts = [f"[bold {colour}]{self.state}[/bold {colour}]"]
        if self.room or self.place:
            where = self.room or f"stage {self.place.get('stage')} area {self.place.get('area')}"
            if self.place:
                where += f"  view {self.place.get('view')}  variant {self.place.get('variant')}  room index {self.place.get('room')}"
            parts.append(escape(where))
        if self.watched:
            parts.append("[dim]watching[/dim] " + escape(", ".join(self.watched)))
        self.query_one("#status", Static).update("   ".join(parts))

    # ---- what is typed

    def on_input_submitted(self, event: Input.Submitted) -> None:
        if event.input.id != "line":
            return
        line = event.value
        box = self.query_one("#line", CommandLine)
        box.value = ""
        if self.asking:                                # the answer to a question: sent as it is, an empty line included
            self.asking = False
            self.query_one("#prompt", Static).remove_class("asking")
            self.show(f"[dim]{escape(line) or '(nothing)'}[/dim]")
            self.send(line)
            return
        if not line.strip():
            return
        self.submit(line)

    def submit(self, line: str) -> None:
        """Carry out a command line, as if it had been typed and entered."""
        self.query_one("#line", CommandLine).remember(line)
        self.show(f"[bold]> {escape(line)}[/bold]")
        word, _, rest = line.strip().partition(" ")
        if word == "bind":                             # this program's own two commands
            self.bind(rest.split() if rest.strip() else self.arguments)
            return
        if word == "unbind":
            self.send("quit")
            return
        self.note_watch(line)
        self.send(line)

    def note_watch(self, line: str) -> None:
        """Keep the top line's list of what is watched in step with the watch commands typed."""
        word, _, rest = line.strip().partition(" ")
        name = rest.strip().split(" ")[0] if rest.strip() else ""
        if word == "watch" and name and name not in self.watched:
            self.watched.append(name)
        elif word == "unwatch":
            self.watched = [w for w in self.watched if name not in ("", "all") and w != name]
        self.show_status()

    def pick_command(self, name: str, arguments: str) -> None:
        """A command taken from the palette: one that needs something more is put on the input line to be
        finished there; any other is carried out at once."""
        box = self.query_one("#line", CommandLine)
        needs = any(not part.startswith("[") for part in arguments.split()) if arguments else False
        if needs:
            box.value = name + " "
            box.cursor_position = len(box.value)
            box.focus()
        else:
            self.submit(name)

    def action_pause(self) -> None:
        self.send("resume" if self.state == "paused" else "pause")

    def action_watching(self) -> None:
        self.send("watching")

    def action_clear(self) -> None:
        self.query_one("#log", RichLog).clear()

    async def action_leave(self) -> None:
        """Have the tracer detach as its `quit` does, wait for it, and leave."""
        if self.bound:
            self.show("[dim]detaching...[/dim]")
            if self.asking:
                self.send("")                          # a command waiting for an answer gets none
            self.send("quit")
            try:
                await asyncio.wait_for(self.process.wait(), 10)
            except asyncio.TimeoutError:
                self.process.terminate()
        self.exit()


def main() -> None:
    arguments = [a for a in sys.argv[1:] if a != "--bind"]
    TraceApp([sys.executable, "-u", str(TRACER), "run", "--machine"], arguments, bind="--bind" in sys.argv[1:]).run()


if __name__ == "__main__":
    main()
