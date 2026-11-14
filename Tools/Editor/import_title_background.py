# 시작 화면 바탕 이미지를 /Game/KGTA/UI/T_TitleBackground 로 가져온다. 원본은 Tools/Source/T_TitleBackground.png.
import unreal
src = unreal.Paths.project_dir() + "Tools/Source/T_TitleBackground.png"
task = unreal.AssetImportTask()
task.set_editor_property("filename", src)
task.set_editor_property("destination_path", "/Game/KGTA/UI")
task.set_editor_property("destination_name", "T_TitleBackground")
task.set_editor_property("replace_existing", True)
task.set_editor_property("automated", True)
task.set_editor_property("save", False)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
t = unreal.load_asset("/Game/KGTA/UI/T_TitleBackground")
t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
t.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
t.set_editor_property("never_stream", True)
t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
unreal.EditorAssetLibrary.save_loaded_asset(t, only_if_is_dirty=False)
open("D:/Dev/import_poster.txt", "w").write("ok %d x %d" % (t.blueprint_get_size_x(), t.blueprint_get_size_y()))
