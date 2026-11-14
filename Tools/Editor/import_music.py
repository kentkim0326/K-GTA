# 배경음악을 /Game/KGTA/Audio 로 가져와 반복 재생으로 둔다. 원본은 Tools/Source/SW_Music_*.wav.
import unreal
out = []
for name in ["SW_Music_Title", "SW_Music_Round"]:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", unreal.Paths.project_dir() + "Tools/Source/%s.wav" % name)
    task.set_editor_property("destination_path", "/Game/KGTA/Audio")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    s = unreal.load_asset("/Game/KGTA/Audio/" + name)
    s.set_editor_property("looping", True)
    unreal.EditorAssetLibrary.save_loaded_asset(s, only_if_is_dirty=False)
    out.append("%s %.1fs looping=%s" % (name, s.get_editor_property("duration"), s.get_editor_property("looping")))
open("D:/Dev/import_music.txt", "w").write("\n".join(out))
