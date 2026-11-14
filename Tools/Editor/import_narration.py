# 시작 화면 내레이션을 /Game/KGTA/Audio/SW_Narration_EN 으로 가져온다. 원본은 Tools/Source/SW_Narration_EN.wav.
import unreal
task = unreal.AssetImportTask()
task.set_editor_property("filename", unreal.Paths.project_dir() + "Tools/Source/SW_Narration_EN.wav")
task.set_editor_property("destination_path", "/Game/KGTA/Audio")
task.set_editor_property("destination_name", "SW_Narration_EN")
task.set_editor_property("replace_existing", True)
task.set_editor_property("automated", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
s = unreal.load_asset("/Game/KGTA/Audio/SW_Narration_EN")
open("D:/Dev/import_narration.txt", "w").write("ok %s %.2fs" % (type(s).__name__, s.get_editor_property("duration")) if s else "FAILED")
