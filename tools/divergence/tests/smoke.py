#!/usr/bin/env python3
"""Opt-in end-to-end test with the real PSX compiler, assembler and scorer.

All artifacts live in a new --output directory. Requires the same executable
permissions and tools as the project build; does not edit a game scratch.
"""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
TOOLS = ROOT / 'tools/divergence'
sys.path.insert(0, str(TOOLS))
from evidence import COMPILER
from intervene import tracer
from report_paths import public_path


def run(argv, cwd, stdout=None):
    result = subprocess.run([str(a) for a in argv], cwd=cwd, capture_output=True, text=True, timeout=60)
    if result.returncode:
        raise RuntimeError(f'{argv[0]} exited {result.returncode}: {result.stdout}\n{result.stderr}')
    if stdout:
        Path(stdout).write_text(result.stdout)
    return result.stdout


def build(source):
    source = Path(source).resolve()
    directory, stem = source.parent, source.stem
    source.with_suffix('.i').write_text(source.read_text())
    run([COMPILER, *tracer().FLAGS, '-o', source.with_suffix('.s'), source.with_suffix('.i')], directory)
    raw = source.with_suffix('.s').read_text()
    raw = re.sub(r'(?m)^([ \t]*#?\.set[ \t]+(?:no)?volatile)[ \t]+#.*$', r'\1', raw)
    asm = source.with_suffix('.assembler.s')
    asm.write_text(raw)
    obj = source.with_suffix('.o')
    run([sys.executable, ROOT / 'tools/maspsx/maspsx.py', '--aspsx-version=2.77', '--run-assembler', '--expand-div',
         '-EL', '-O2', '-march=r3000', '-mtune=r3000', '-no-pad-sections', '-G0', '-o', obj, asm], ROOT)
    run([sys.executable, ROOT / 'tools/claude-decomp-env/objdump.py', obj], ROOT,
        directory / f'{stem}_object_dump_normalized.s')
    if stem != 'target':
        run([sys.executable, ROOT / 'tools/claude-decomp-env/dist.py', directory / 'target.o', obj,
             '--stack-diffs', '--json', source.with_suffix('.score.json')], ROOT)


def smoke(output):
    checks = json.loads(Path(__file__).with_name('compiler_checks.json').read_text())
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    scratch = output / 'fixture'
    scratch.mkdir()
    seed = checks['sources']['base']
    for stem in ('target', 'base', 'improved'):
        source = checks['sources'][stem]
        path = scratch / f'{stem}.c'
        path.write_text(source)
        build(path)
    script = f'exec {shlex.quote(sys.executable)} -B {shlex.quote(str(Path(__file__).resolve()))} --build "$1"\n'
    (scratch / 'build.sh').write_text('#!/bin/sh\nset -eu\n' + script)
    plan = output / 'plan.json'
    plan.write_text(json.dumps(checks['plan']))
    cli = [sys.executable, '-B', TOOLS / 'toolset.py']
    for command in ('probe', 'search'):
        run([*cli, command, scratch, 'base', plan, '--output', output / command, '--budget', '4', '--function', checks['function']], ROOT,
            output / f'{command}.log')
        manifest = json.loads((output / command / 'manifest.json').read_text())
        assert any(row.get('score', {}).get('distance') == 0 and all(c['status'] == 'met' for c in row['constraints'])
                   for row in manifest['experiments']), manifest
        assert 1 <= manifest['used'] <= 4
    run([*cli, 'minimize', scratch, 'base', 'improved', '--output', output / 'minimize', '--budget', '6', '--function', checks['function']], ROOT,
        output / 'minimize.log')
    result = json.loads((output / 'minimize/reduction.json').read_text())
    assert result['distance'] == 0 and 'unused' not in (output / 'minimize/reduced.c').read_text(), result
    assert (scratch / 'base.c').read_text() == seed
    (output / 'result.json').write_text(json.dumps({'passed': ['probe', 'search', 'minimize', 'original_source_unchanged'],
                                                  'scope': 'Tiny arithmetic fixture compiled with the bundled compiler and independently scored.'}, indent=2) + '\n')
    print(f'Real compiler workflow passed: {public_path(output / "result.json")}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--build', type=Path)
    args = parser.parse_args()
    if args.build:
        build(args.build)
    elif args.output:
        smoke(args.output)
    else:
        parser.error('--output is required')
