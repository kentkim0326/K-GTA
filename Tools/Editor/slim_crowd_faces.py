# 외형 설정에 쓰인 크라우드 얼굴 메시를 가볍게 한다. 제출 zip 1 GB 제한 때문이다.
#  - RigLogic DNA(표정 리그 데이터)를 뗀다. 행인 표정은 쓰지 않는다.
#  (LOD0~1 삭제는 remove_lods 가 크래시해서 뺐다. DNA 만 떼도 얼굴 하나가 91 MB → 18 MB.)
import re, unreal
lib = unreal.EditorSkeletalMeshLibrary
text = open(unreal.Paths.project_config_dir() + "DefaultGame.ini", encoding="utf-8-sig").read()
faces = sorted(set(p.split(".")[0] for p in re.findall(r'Face="(/Game/[^"]+)"', text)))
out = []
for path in faces:
    m = unreal.load_asset(path)
    before = lib.get_lod_count(m)
    data = list(m.get_editor_property("asset_user_data") or [])
    kept = [d for d in data if d and "DNA" not in d.get_class().get_name()]
    m.set_editor_property("asset_user_data", kept)
    removed = False
    if False:  # remove_lods 는 이 메시에서 에디터를 크래시시킨다(2026-11-14). LOD 는 그대로 둔다.
        removed = lib.remove_lods(m, [0, 1])
        mp = m.get_editor_property("min_lod"); mp.default = 0; m.set_editor_property("min_lod", mp)
    unreal.EditorAssetLibrary.save_loaded_asset(m, only_if_is_dirty=False)
    out.append("%s userdata %d->%d lods %d->%d removed=%s verts0=%d" % (path.split("/")[-1], len(data), len(kept), before, lib.get_lod_count(m), removed, lib.get_num_verts(m, 0)))
open("D:/Dev/slim_faces.txt", "w").write("\n".join(out))
