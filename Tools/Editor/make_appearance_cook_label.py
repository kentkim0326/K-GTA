# 외형 설정(DefaultGame.ini 의 GunDayCore 섹션)에 적힌 자산을 쿠킹에 넣는 라벨을 만든다.
# 설정의 soft 참조는 쿠커가 따라가지 않기 때문이다. 외형 설정을 고치면 다시 돌린다.
#   UnrealEditor-Cmd GunsKorea.uproject -run=pythonscript -script=Tools/Editor/make_appearance_cook_label.py
import re, unreal
ini = unreal.Paths.project_config_dir() + "DefaultGame.ini"
text = open(ini, encoding="utf-8-sig").read()
section = text.split("[/Script/GunDayCore.GunDayCoreSettings]", 1)[1].split("\n[", 1)[0]
keys = ("UE4MannequinSkeleton", "MannyMesh", "UE4ToUE5RetargetAnimClass", "UE5ToMetaHumanRetargetAnimClass", "FaceAnimClass", "+AppearanceRules")
lines = [l for l in section.splitlines() if l.startswith(keys)]
paths = set()
for l in lines:
    for p in re.findall(r'"(/Game/[^"]+)"', l):
        pkg = p.split(".")[0]
        if "/AIPresets/" in pkg or "/PlayerCharacters/" in pkg:
            continue  # 폰 클래스는 맵과 다른 설정이 이미 쿠킹한다
        paths.add(pkg)
assets = []
for pkg in sorted(paths):
    o = unreal.load_asset(pkg)
    if o: assets.append(o)
    else: unreal.log_warning("라벨: 못 찾음 " + pkg)
folder, name = "/Game/KGTA/Cook", "PAL_GunDayAppearance"
full = folder + "/" + name
label = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else None
if not label:
    label = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.PrimaryAssetLabel, unreal.DataAssetFactory())
rules = label.get_editor_property("rules")
rules.set_editor_property("cook_rule", unreal.PrimaryAssetCookRule.ALWAYS_COOK)
label.set_editor_property("rules", rules)
label.set_editor_property("label_assets_in_my_directory", False)
label.set_editor_property("explicit_assets", assets)
unreal.EditorAssetLibrary.save_loaded_asset(label, only_if_is_dirty=False)
open("D:/Dev/label_result.txt", "w", encoding="utf-8").write("assets %d of %d\n" % (len(assets), len(paths)) + "\n".join(sorted(paths)))
