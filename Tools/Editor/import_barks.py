# 행인·형사 외침을 /Game/KGTA/Audio/Barks 로 가져온다. 원본은 Tools/Source/Barks/SW_Bark_*.wav (README 에 한글 원래 이름).
import glob, os, unreal
src_dir = unreal.Paths.project_dir() + "Tools/Source/Barks/"
out = []
for path in sorted(glob.glob(src_dir + "SW_Bark_*.wav")):
    name = os.path.splitext(os.path.basename(path))[0]
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", "/Game/KGTA/Audio/Barks")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    s = unreal.load_asset("/Game/KGTA/Audio/Barks/" + name)
    out.append("%s %.2fs" % (name, s.get_editor_property("duration")) if s else name + " FAILED")
open("D:/Dev/import_barks.txt", "w").write("\n".join(out))
