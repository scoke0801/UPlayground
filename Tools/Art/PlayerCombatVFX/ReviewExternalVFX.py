"""Make a labeled contact sheet from real combat captures for visual review."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('run', type=Path)
parser.add_argument('--build', type=int, default=0)
args = parser.parse_args()
# Windows drawing APIs avoid an extra Python package in the UE toolchain.
subprocess.run(['powershell.exe','-NoProfile','-File',str(Path(__file__).with_suffix('.ps1')),
                '-Run',str(args.run.resolve()),'-Build',str(args.build)],check=True)
