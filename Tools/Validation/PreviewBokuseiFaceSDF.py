"""Use the saved eight-stage fixture to render paired SDF light sweeps and test PIE."""
import sys
from pathlib import Path
import unreal
assert '-PGFaceSDFProbe' in unreal.SystemLibrary.get_command_line(), 'Use RunBokuseiFaceSDF.py --step preview'
sys.path.insert(0,str(Path(unreal.Paths.project_dir()).resolve()/'Tools/Validation'))
import PreviewBokuseiShadingComparison
