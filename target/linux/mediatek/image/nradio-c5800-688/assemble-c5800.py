#!/usr/bin/env python3
"""Use the maintained C5800 assembler for both build entry points."""
from pathlib import Path
import runpy

runpy.run_path(str(Path(__file__).resolve().parents[1] / 'assemble-c5800.py'), run_name='__main__')
