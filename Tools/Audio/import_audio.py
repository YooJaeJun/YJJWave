"""생성한 WAV를 언리얼 사운드 에셋으로 만든다 (에디터 Python, 커맨드릿에서 실행: Tools/Audio/BuildAudio.ps1).

  /Game/YJJWave/Audio/<이름>   BGM_* 는 반복 재생
여러 번 실행해도 같은 결과 (있으면 갱신), 생성기에서 빠진 소리는 지운다.
"""
import os

import unreal

AUDIO_DIR = "/Game/YJJWave/Audio"
PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE_DIR = os.path.join(PROJECT_DIR, "Tools", "Audio", "Generated")

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary


def log(message):
    unreal.log("[YJJAudio] " + message)


def main():
    wanted = set()
    for file_name in sorted(os.listdir(SOURCE_DIR)):
        if not file_name.endswith(".wav"):
            continue
        name = file_name[:-4]
        wanted.add(name)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(SOURCE_DIR, file_name))
        task.set_editor_property("destination_path", AUDIO_DIR)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        sound = assets.load_asset(AUDIO_DIR + "/" + name)
        if sound is None:
            raise RuntimeError("Failed to import " + file_name)
        sound.set_editor_property("looping", name.startswith("BGM_"))
        log("sound " + name)
    for path in assets.list_assets(AUDIO_DIR, recursive=False):
        name = path.split("/")[-1].split(".")[0]
        if name not in wanted:
            assets.delete_asset(path.split(".")[0])
            log("deleted " + name)
    assets.save_directory(AUDIO_DIR, only_if_is_dirty=False, recursive=True)
    log("done")


main()
