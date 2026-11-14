# 상황별 목소리(경찰·아줌마·행인 남자·시비)를 /Game/KGTA/Audio/Voices 로 가져온다. 원본은 Tools/Source/Voices/SW_Voice_*.wav (README 에 한글 원래 이름).
import glob, os, unreal
src_dir = unreal.Paths.project_dir() + "Tools/Source/Voices/"
out = []
for path in sorted(glob.glob(src_dir + "SW_Voice_*.wav")):
    name = os.path.splitext(os.path.basename(path))[0]
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", "/Game/KGTA/Audio/Voices")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    s = unreal.load_asset("/Game/KGTA/Audio/Voices/" + name)
    out.append("%s %.2fs" % (name, s.get_editor_property("duration")) if s else name + " FAILED")
open("D:/Dev/import_voices.txt", "w").write("\n".join(out))
