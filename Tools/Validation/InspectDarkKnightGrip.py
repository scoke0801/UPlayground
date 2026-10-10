import json,os
from pathlib import Path
import unreal
out=Path(os.environ['PG_DARK_KNIGHT_RUN'])
mesh=unreal.load_asset('/Game/ExternalAssets/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SM_DKM_Sword')
task=unreal.AssetExportTask();task.object=mesh;task.filename=str(out/'sword.obj')
task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.StaticMeshExporterOBJ()
assert unreal.Exporter.run_asset_export_task(task)
rig=unreal.load_asset('/Game/DataCenter/DarkKnightBoss/IK_DarkKnight')
ctl=unreal.IKRigController.get_controller(rig)
report=dict(bounds=str(mesh.get_bounds()),hand_r=ctl.get_ref_pose_transform_of_bone('hand_r').export_text())
(out/'grip.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('PGDarkKnight grip PASS '+json.dumps(report))
