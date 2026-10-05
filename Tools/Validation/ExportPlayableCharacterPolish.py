"""Read-only editor export. The external runner backs up and adopts the JSON."""
import os
import sys
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(root/'Tools/Validation'))
from PlayableCharacterPolish import export_source
from PlayableCharacterTransaction import write_json

output = Path(os.environ['PG_CHARACTER_RUN'])
write_json(output/'export.json', export_source())
