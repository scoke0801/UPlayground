import unreal, os
root=unreal.Paths.project_dir()
for script in ['ConfigureCombatCycleAssets.py','ValidateCombatCycleAssets.py']:
    path=os.path.join(root,'Tools','Validation',script)
    exec(compile(open(path,encoding='utf-8-sig').read(),path,'exec'))
