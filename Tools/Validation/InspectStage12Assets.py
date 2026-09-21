"""Read-only Unreal Python audit; run with -run=pythonscript -script=<this path>."""
import json
import os
import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
result = {"tables": {}, "blueprints": {}, "maps": []}
for item in registry.get_assets_by_path("/Game/DataCenter/DataTables", recursive=True):
    asset = item.get_asset()
    if isinstance(asset, unreal.DataTable):
        result["tables"][str(item.package_name)] = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(asset))
for item in registry.get_assets_by_path("/Game/Blueprints", recursive=True):
    name = str(item.asset_name)
    if any(term in name.lower() for term in ("player", "gamemode", "startup")):
        result["blueprints"][str(item.package_name)] = str(item.asset_class_path)
for item in registry.get_assets_by_path("/Game/Maps", recursive=True):
    if str(item.asset_class_path.asset_name) == "World":
        result["maps"].append(str(item.package_name))
out = os.path.join(unreal.Paths.project_saved_dir(), "Stage12AssetAudit.json")
with open(out, "w", encoding="utf-8") as handle:
    json.dump(result, handle, ensure_ascii=False, indent=2)
unreal.log("PG asset audit saved: " + out)