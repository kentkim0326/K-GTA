# 크라우드 얼굴 머티리얼이 SM5 에서 텍스처 슬롯(SRV) 66개를 써서 64개 한도를 넘는다.
# 컴파일이 실패하면 쿠킹된 게임에서 얼굴이 회색 기본 머티리얼로 나온다.
# 외형 설정에 쓰인 얼굴의 머티리얼 인스턴스에서 텍스처를 쓰는 부가 기능 둘을 끈다.
import re, unreal
mel = unreal.MaterialEditingLibrary
OFF = ["Detail Normals", "BakedRoughnessModulation"]
text = open(unreal.Paths.project_config_dir() + "DefaultGame.ini", encoding="utf-8-sig").read()
faces = sorted(set(p.split(".")[0] for p in re.findall(r'Face="(/Game/[^"]+)"', text)))
reg = unreal.AssetRegistryHelpers.get_asset_registry()
out = []
for face in faces:
    folder = face.rsplit("/Face/", 1)[0] + "/Materials"
    mics = reg.get_assets(unreal.ARFilter(package_paths=[folder], recursive_paths=True, class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "MaterialInstanceConstant")]))
    n = 0
    for a in mics:
        mi = unreal.load_asset(str(a.package_name))
        names = mel.get_static_switch_parameter_names(mi.get_base_material())
        if not any(o in names for o in OFF): continue
        for o in OFF:
            if o in names: mel.set_material_instance_static_switch_parameter_value(mi, o, False)
        mel.update_material_instance(mi)
        unreal.EditorAssetLibrary.save_loaded_asset(mi, only_if_is_dirty=False)
        n += 1
    out.append("%s: %d instances" % (folder, n))
open("D:/Dev/fix_head.txt", "w").write("\n".join(out))
