# 골목 측정용 맵 준비.
#
# 에디터 출력 로그(Cmd)에서 실행한다. 실행 전에 열린 레벨을 저장해 둘 것.
#
#   py "C:/Dev/GunsKorea/Tools/Editor/setup_alley_map.py"
#       Night_Demo 를 KGTA 로 복제하고 연 뒤, GameMode 지정과 NavMeshBoundsVolume 배치까지 한다.
#
#   py "C:/Dev/GunsKorea/Tools/Editor/setup_alley_map.py" start
#       지금 뷰포트 카메라 아래 바닥에 PlayerStart 를 놓는다. 골목으로 날아간 뒤 부른다.
#
#   py "C:/Dev/GunsKorea/Tools/Editor/setup_alley_map.py" spot 4 6
#       카메라 아래 바닥에 시비 지점을 놓고 4번, 6번 상황을 붙인다. 번호를 빼면 아무 상황이나 난다.
#       배역 둘은 카메라 기준 좌우로 마주 선다. 옆모습이 보이도록 카메라를 돌려 두고 부른다.
#       상황 번호(프로젝트 세팅 > GunDay Core > 시비 > Dispute Scenarios 순서):
#         우발  0 주차 시비  1 담배 훈계  2 노인석  3 편의점
#         대기  4 병원 대기실  5 전세 사기  6 학폭 처리  7 산재 은폐  8 보험금 거절

import sys
import unreal

SOURCE_MAP = "/Game/Tokyo_Street/Maps/Night_Demo"
TARGET_MAP = "/Game/KGTA/Maps/KGTA_Alley_Night"
GAME_MODE = "/Game/ThirdPersonKit/Blueprints/BP_TPS_Game_Mode"

# 경찰은 시야 밖 9~25m 에 투입된다. 맵 가장자리에서도 투입 자리가 나오도록 여유를 둔다.
NAV_MARGIN = 3000.0
# 박스 볼륨 팩토리가 만드는 기본 큐브 한 변(언리얼 단위).
DEFAULT_BRUSH_SIZE = 200.0

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(msg):
    unreal.log("[골목 준비] " + msg)


def open_target_map():
    # duplicate_asset 으로 복제한 뒤 load_level 로 열면, 메모리에 남은 복제 월드를
    # 치우지 못해 에디터가 죽는다(World Memory Leaks). 원본을 열고 다른 이름으로 저장한다.
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET_MAP):
        log("이미 있어 복제를 건너뜀: " + TARGET_MAP)
        if not level_editor.load_level(TARGET_MAP):
            raise RuntimeError("레벨을 열지 못함: " + TARGET_MAP)
        return editor.get_editor_world()

    if not level_editor.load_level(SOURCE_MAP):
        raise RuntimeError("레벨을 열지 못함: " + SOURCE_MAP)
    if not unreal.EditorLoadingAndSavingUtils.save_map(editor.get_editor_world(), TARGET_MAP):
        raise RuntimeError("다른 이름으로 저장 실패: " + TARGET_MAP)

    world = editor.get_editor_world()
    if not world.get_path_name().startswith(TARGET_MAP):
        raise RuntimeError("저장 뒤 열린 레벨이 복제본이 아님: " + world.get_path_name())
    log("복제함: " + TARGET_MAP)
    return world


def set_game_mode(world):
    game_mode = unreal.EditorAssetLibrary.load_blueprint_class(GAME_MODE)
    if game_mode is None:
        raise RuntimeError("GameMode 를 찾지 못함: " + GAME_MODE)
    settings = unreal.GameplayStatics.get_actor_of_class(world, unreal.WorldSettings)
    settings.set_editor_property("default_game_mode", game_mode)
    log("GameMode Override: " + GAME_MODE)


def level_bounds():
    # 하늘, 안개 같은 무한 크기 액터를 빼려고 랜드스케이프와 스태틱 메시만 본다.
    lo = [float("inf")] * 3
    hi = [float("-inf")] * 3
    count = 0
    for actor in actors.get_all_level_actors():
        if not isinstance(actor, (unreal.LandscapeProxy, unreal.StaticMeshActor)):
            continue
        origin, extent = actor.get_actor_bounds(False)
        for i, axis in enumerate("xyz"):
            o = getattr(origin, axis)
            e = getattr(extent, axis)
            lo[i] = min(lo[i], o - e)
            hi[i] = max(hi[i], o + e)
        count += 1
    if count == 0:
        raise RuntimeError("범위를 잴 액터가 없음")
    return lo, hi


def place_nav_bounds():
    existing = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.NavMeshBoundsVolume)]
    if existing:
        log("NavMeshBoundsVolume 이 이미 {}개 있어 건너뜀".format(len(existing)))
        return

    lo, hi = level_bounds()
    center = unreal.Vector(*[(lo[i] + hi[i]) / 2 for i in range(3)])
    size = [hi[i] - lo[i] + NAV_MARGIN * 2 for i in range(3)]

    volume = actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, center)
    volume.set_actor_scale3d(unreal.Vector(*[s / DEFAULT_BRUSH_SIZE for s in size]))
    volume.set_actor_label("NavBounds_Alley")

    _, extent = volume.get_actor_bounds(False)
    if extent.x < 1000:
        unreal.log_warning("[골목 준비] 볼륨 브러시가 비어 있다. 디테일 패널에서 Brush Settings 를 직접 키울 것.")
    log("NavMeshBoundsVolume: 중심 {}, 크기 {:.0f} x {:.0f} x {:.0f} m".format(
        center, size[0] / 100, size[1] / 100, size[2] / 100))


def floor_under_camera():
    """뷰포트 카메라 바로 아래 바닥 위치와 카메라가 보는 방향(yaw)을 돌려준다."""
    world = editor.get_editor_world()
    cam_loc, cam_rot = editor.get_level_viewport_camera_info()
    end = cam_loc + unreal.Vector(0, 0, -100000)
    hit = unreal.SystemLibrary.line_trace_single(
        world, cam_loc, end, unreal.TraceTypeQuery.ECC_VISIBILITY,
        False, [], unreal.DrawDebugTrace.NONE, True)
    # 맞은 것이 없으면 None 이 온다. to_tuple 은 Break Hit Result 순서다.
    # 0 = 막혔는가, 4 = 맞은 위치. (UE 5.8 에서 확인)
    parts = hit.to_tuple() if hit is not None else None
    if not parts or not parts[0]:
        raise RuntimeError("카메라 아래에 바닥이 없음. 골목 위로 카메라를 옮긴 뒤 다시 부를 것.")
    return parts[4], cam_rot.yaw


def place_player_start():
    for a in actors.get_all_level_actors():
        if isinstance(a, unreal.PlayerStart):
            actors.destroy_actor(a)
            log("기존 PlayerStart 를 지움")

    floor, yaw = floor_under_camera()
    # 캡슐 반 높이만큼 띄워 BADSIZE 를 피한다.
    location = unreal.Vector(floor.x, floor.y, floor.z + 100)
    actors.spawn_actor_from_class(unreal.PlayerStart, location, unreal.Rotator(0, 0, yaw))
    log("PlayerStart: {} (카메라가 보던 방향)".format(location))


def place_dispute_spot(indices):
    # 설정 클래스가 Python 에 노출되어 있지 않아 번호를 여기서 검사하지 못한다.
    # 없는 번호면 게임 중에 "시비 배역: ... 상황이 설정에 없다" 경고가 뜬다.
    # 지점은 바닥에 둔다. 배역은 지점의 좌우로 서므로 카메라 방향을 그대로 쓰면 옆모습이 보인다.
    floor, yaw = floor_under_camera()
    spot = actors.spawn_actor_from_class(unreal.GunDayDisputeSpot, floor, unreal.Rotator(0, 0, yaw))
    spot.set_editor_property("scenario_indices", indices)

    label = "DisputeSpot_" + ("_".join(str(i) for i in indices) if indices else "Any")
    spot.set_actor_label(label)
    log("시비 지점 {}: {}".format(label, floor))


def main():
    command = sys.argv[1] if len(sys.argv) > 1 else ""
    if command == "start":
        place_player_start()
    elif command == "spot":
        place_dispute_spot([int(a) for a in sys.argv[2:]])
    else:
        world = open_target_map()
        set_game_mode(world)
        place_nav_bounds()
        log("다음: 골목 위로 카메라를 옮기고 'start' 인자로 다시 부를 것")
    level_editor.save_current_level()
    log("저장함")


main()
