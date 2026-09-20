#!/usr/bin/env python3
import os
import sys
from pathlib import Path

root = Path(__file__).resolve().parent
sys.path.insert(0, str(root / "src"))
os.chdir(root)

from smo.cli import main

if __name__ == "__main__":
    raise SystemExit(main())
