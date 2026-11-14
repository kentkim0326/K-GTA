# CitySampleCrowd 텍스처의 최대 크기를 1024 로 묶는다. 제출 zip 1 GB 제한 때문이다.
# 크라우드 텍스처는 가상 텍스처이고 PROJECT02 같은 그룹이라 기기 프로필의 1024 제한이 안 걸린다.
import unreal
LIMIT = 1024
reg = unreal.AssetRegistryHelpers.get_asset_registry()
assets = reg.get_assets(unreal.ARFilter(package_paths=["/Game/CitySampleCrowd"], recursive_paths=True, class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "Texture2D")]))
changed = 0
for a in assets:
    t = unreal.load_asset(str(a.package_name))
    if not t: continue
    cur = t.get_editor_property("max_texture_size")
    if cur == 0 or cur > LIMIT:
        t.set_editor_property("max_texture_size", LIMIT)
        unreal.EditorAssetLibrary.save_loaded_asset(t, only_if_is_dirty=False)
        changed += 1
open("D:/Dev/shrink_result.txt", "w").write("textures %d, changed %d\n" % (len(assets), changed))
