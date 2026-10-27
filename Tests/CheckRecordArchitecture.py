"""Compatibility entry point for existing validation scripts."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name("CheckArchitecture.py")), run_name="__main__")
