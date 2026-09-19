#!/usr/bin/env python3
"""Compatibility entry point for comparing compiler state between attempts.

A change in internal state is evidence of influence, not necessarily progress.
The comparison uses expression correspondence rather than equal pseudo IDs.
"""
from compare import compare, main, render

if __name__ == '__main__':
    raise SystemExit(main())
