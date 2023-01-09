# How to use
# ==========
# pip install pipenv
# pipenv install -d
# pipenv run python prepare_assets.py

# Quelle https://thepythoncorner.com/posts/2019-01-13-how-to-create-a-watchdog-in-python-to-look-for-filesystem-changes/
# We need python 3.10 for cross platform newline

# Export to exe on windows via
# pyinstaller --onefile .\prepare_assets.py

import json
import os
import shutil
import subprocess
import sys
import time
from multiprocessing import Pool, freeze_support
from pathlib import Path
from stat import S_IREAD, S_IRGRP, S_IROTH, S_IWUSR

from termcolor import colored
from watchdog.events import PatternMatchingEventHandler
from watchdog.observers import Observer

SCHNACKER_FOLDER = "data/dialog/"
RHUBARB_OUT = "data/rhubarb/"

SPINE_THREADS = os.cpu_count()

set_read_only = True
# could be "linux", "linux2", "linux3", ...
if sys.platform.startswith("linux"):
    RHUBARB = None
    SPINE = "/usr/bin/spine"
    LUA = "luac"
elif sys.platform == "darwin":
    RHUBARB = '/Applications/Rhubarb-Lip-Sync-1.13.0-macOS/rhubarb'
    SPINE = '/Applications/Spine.app/Contents/MacOS/Spine'
    LUA = 'luac'
elif sys.platform == "win32":
    # Windows
    RHUBARB = 'windows_bin\\rhubarb.exe'
    SPINE = 'C:\\Program Files\\Spine\\Spine.exe'
    LUA = 'windows_bin\\luac.exe'
    MAGICK = 'windows_bin\\magick.exe'
    set_read_only = False


class Progress(object):
    def __init__(self, initialCount: int, maxCount: int, maxTitleLength: int = 26, barLength: int = 46):
        self._initialCount = initialCount
        self._currentCount = initialCount
        self._maxCount = maxCount
        self._title = ""
        self._maxTitleLength = maxTitleLength
        self._barLength = barLength
        self._errors = []
        self._warnings = []

    def updateTitle(self, title: str):
        self._title = title
        self.print()

    def advance(self, count=1):
        self._currentCount += count
        self.print()

    def print(self):
        if self._maxCount == 0:
            return
        percent = float(self._currentCount) / self._maxCount
        printedTitle = (self._title if len(self._title) <= self._maxTitleLength
                        else "[...]" + self._title[len(self._title) - self._maxTitleLength - 5:])

        filledCount = int(self._barLength * percent)
        filled = "█" * filledCount
        unfilled = "-" * (self._barLength - filledCount)
        percentage = "%3.1f" % (percent * 100)
        print(colored(f"\r{printedTitle} |{filled}{unfilled}| {percentage}%",
                      "red" if len(self._errors) > 0
                      else "yellow" if len(self._warnings) > 0
                      else "blue"), end="\r")

    def finish(self):
        print("\n")

    def addError(self, errorMsg: str):
        self._errors.append(errorMsg)
        self.print()

    def addWarning(self, warnMsg: str):
        self._warnings.append(warnMsg)
        self.print()


def applyConvertToDirectory(dir):
    for folder in dir:
        for root, dirs, files in os.walk(folder):
            for file in files:
                if file.endswith(".png") or file.endswith(".jpg"):
                    applyConvert(root + '/' + file)


def deletePngToDirectory(dir):
    for folder in dir:
        for root, dirs, files in os.walk(folder):
            for file in files:
                if file.endswith(".png") or file.endswith(".jpg"):
                    os.remove(root + '/' + file)


def replacePngTextWithWebP(dir):
    for folder in dir:
        for root, dirs, files in os.walk(folder):
            for file in files:
                if file.endswith(".atlas") or file.endswith(".json"):
                    _file = Path(root + "/" + file)
                    if set_read_only:
                        # Make file writeable
                        os.chmod(_file, S_IREAD | S_IRGRP | S_IROTH | S_IWUSR)
                    _file.write_text(_file.read_text(encoding='utf_8').replace(
                        '.png', '.webp'), encoding='utf_8', newline='\r\n')
                    if set_read_only:
                        # Make file read only
                        os.chmod(_file, S_IREAD | S_IRGRP | S_IROTH)


def applyConvert(dir):
    # Make file writeable
    if set_read_only:
        try:
            os.chmod(dir[:-4] + '.webp', S_IREAD | S_IRGRP | S_IROTH | S_IWUSR)
        except Exception:
            pass
    if sys.platform != "win32":
        fullCommand = 'convert ' + dir + ' ' + dir[:-4] + '.webp'
    else:
        fullCommand = f"{MAGICK} {dir} {dir[:-4]}.webp"

    print(colored(fullCommand, 'blue'))
    os.system(fullCommand)
    # Make file read only
    if set_read_only:
        os.chmod(dir[:-4] + '.webp', S_IREAD | S_IRGRP | S_IROTH)


def convert_png_to_webp():
    applyConvertToDirectory(['./data/'])
    deletePngToDirectory(['./data/'])
    replacePngTextWithWebP(['./data/'])


def rhubarb_export(node_id):
    errors = []
    warnings = []
    if not RHUBARB:
        return errors, warnings
    rhubarb_out = f"{RHUBARB_OUT}{node_id}.json"
    if set_read_only:
        os.chmod(rhubarb_out, S_IREAD | S_IRGRP | S_IROTH | S_IWUSR)
    command = [RHUBARB, f"data/audio/{node_id}.ogg", "-r", "phonetic", "-f", "json", "-o", rhubarb_out]
    p = subprocess.Popen(command, stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = p.communicate()[0]
    if p.returncode != 0:
        errors.append(output.decode())
    if set_read_only:
        os.chmod(rhubarb_out, S_IREAD | S_IRGRP | S_IROTH)
    return errors, warnings


def rhubarb_reexport():
    nodes = []
    for _root, _dirs, files in os.walk(SCHNACKER_FOLDER):
        for file in files:
            if file.endswith(".schnack"):
                with open(_root + file) as dialog_file:
                    dialogs = json.load(dialog_file)
                    for dialog in dialogs["dialogs"]:
                        for node in dialog["nodes"]:
                            character_name = ''
                            if "character" not in node or node["character"] in ["char_player"]:
                                character_name = 'joy'
                            elif "character" not in node or node["character"] in ["char_dog"]:
                                character_name = 'dog'
                            else:
                                character_name = node["character"]

                            if "character" not in node or node["character"] in ["marc"]:
                                continue  # Wir haben Marc noch nicht erstellt

                            if "text" in node:
                                node_id = str(node["id"]).zfill(3)

                                if not os.path.exists(f"data/audio/{node_id}.ogg"):
                                    print("Can not load: ", f"data/audio/{node_id}.ogg")
                                    continue

                                nodes.append(node_id)

    progress = Progress(0, len(nodes))
    progress.updateTitle(" Re-Exporting Rhubarb Files")
    results = []
    with Pool(SPINE_THREADS) as p:
        for i, (errors, warnings) in enumerate(p.imap_unordered(rhubarb_export, nodes), 0):
            progress.advance()

            if len(errors) > 0 or len(warnings) > 0:
                results.append({
                    "file": nodes[i],
                    "errors": errors,
                    "warnings": warnings,
                })
    progress.finish()

    for node_id in nodes:
        rhubarb_out = f"{RHUBARB_OUT}{node_id}.json"

        # read rhubarb output
        with open(rhubarb_out) as rhubarb_outfile:
            # Add animation to character
            character_path = f"data/{character_name}/{character_name}.json"

            if not os.path.exists(character_path):
                print("Can not write into: ", character_path)
                continue
            if set_read_only:
                os.chmod(character_path, S_IREAD | S_IRGRP | S_IROTH | S_IWUSR)
            with open(character_path, 'r+') as character_file:
                mouthCues = json.load(rhubarb_outfile)
                character = json.load(character_file)
                animation = [{"time": cues['start'],
                              "name": f"front-mouth-{str(cues['value']).lower()}"} for cues in mouthCues['mouthCues']]

                character["animations"][f"say_{node_id}"] = {}
                character["animations"][f"say_{node_id}"]["slots"] = {}
                character["animations"][f"say_{node_id}"]["slots"]["mouth"] = {}
                character["animations"][f"say_{node_id}"]["slots"]["mouth"]["attachment"] = animation

                character_file.seek(0)
                character_file.write(json.dumps(character))
                pass

            if set_read_only:
                os.chmod(character_path, S_IREAD | S_IRGRP | S_IROTH)


def spine_reexport(dir):
    spinefiles = []
    for folder in dir:
        for root, dirs, files in os.walk(folder):
            for file in files:
                if file.endswith(".spine"):
                    # spine_export(file=root + '/' + file)
                    spinefiles.append(root + '/' + file)

    progress = Progress(0, len(spinefiles))
    progress.updateTitle(" Re-Exporting Spine Files")
    results = []
    with Pool(SPINE_THREADS) as p:
        for i, (errors, warnings) in enumerate(p.imap_unordered(spine_export, spinefiles), 0):
            progress.advance()

            if len(errors) > 0 or len(warnings) > 0:
                results.append({
                    "file": spinefiles[i],
                    "errors": errors,
                    "warnings": warnings,
                })
    progress.finish()

    for result in results:
        printErrorsAndWarnings(
            result["file"], result["errors"], result["warnings"])


def printErrorsAndWarnings(fileName, errors, warnings):
    color = "red" if len(errors) > 0 else "yellow"
    print(colored(f"  {fileName}", color))
    for error in errors:
        print(colored(f"    ERROR: {error}", "red"))
    for warning in warnings:
        print(colored(f"    WARNING: {warning}", "yellow"))
    print()


# there is no print allowed in this function, since this would destroy the progress bar
def spine_export(file: str):
    errors = []
    warnings = []
    if not file.endswith('.spine'):
        errors.append("invalid spine file " + file)
        return errors, warnings

    if not os.path.exists(SPINE):
        errors.append("spine executable '" + SPINE + "' could not be found!")
        return errors, warnings

    name = os.path.splitext(Path(file).name)[0]
    command = [SPINE, '-i', file, '-m', '-o',
               f"./data/{name}/", '-e', './data-src/spine_export_template.export.json']
    # print(colored(' '.join(command), 'blue'))
    try:
        shutil.rmtree(f"./data/{name}/", ignore_errors=True)
        if set_read_only:
            os.chmod(f"./data/{name}/{name}.json",
                     S_IREAD | S_IRGRP | S_IROTH | S_IWUSR)
            os.chmod(f"./data/{name}/{name}.atlas",
                     S_IREAD | S_IRGRP | S_IROTH | S_IWUSR)
    except Exception:
        pass
        # print(colored(f"Error set ./data/{name}/{name}.json writable: {e}", 'red'))
    p = subprocess.Popen(command, stdout=subprocess.PIPE,
                         stdin=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = p.communicate()[0]
    if p.returncode != 0:
        print(colored(output.decode(), 'red'))
        exit(p.returncode)

    if not os.path.exists(f"./data/{name}/{name}.json"):
        errors.append(
            f"Spine export of {name} failed. No file ./data/{name}/{name}.json was created. \nIs the sceleton of {name}.spine named {name}?")
        return errors, warnings

    # read spine json to check for missing scripts
    with open(f"./data/{name}/{name}.json") as spine_json:
        spine_object = json.load(spine_json)
        if "skins" in spine_object:
            for i, _skin in enumerate(spine_object["skins"]):

                if "attachments" not in spine_object["skins"][i]:
                    continue

                for attachment in spine_object["skins"][i]["attachments"].keys():
                    for subattachment in spine_object["skins"][i]["attachments"][attachment].keys():
                        if "name" in spine_object["skins"][i]["attachments"][attachment][subattachment]:
                            bbname = spine_object["skins"][i]["attachments"][attachment][subattachment]["name"]
                        else:
                            bbname = subattachment

                        if "type" in spine_object["skins"][i]["attachments"][attachment][subattachment] \
                                and spine_object["skins"][i]["attachments"][attachment][subattachment]["type"] == "boundingbox":
                            if bbname == "walkable_area":  # No scripts for navmeshes
                                continue
                            if not bbname.startswith("dlg:") and not os.path.exists(f"./data-src/scripts/{bbname}.lua"):
                                if not os.path.exists("./data-src/scripts/"):
                                    Path("./data-src/scripts/").mkdir(parents=True, exist_ok=True)
                                with open(f"./data-src/scripts/{bbname}.lua", 'w') as f:
                                    f.write(f'print("{bbname}")')
                                warnings.append(f"Script {bbname}.lua was created automatically!")

    if set_read_only:
        try:
            os.chmod(f"./data/{name}/{name}.json", S_IREAD | S_IRGRP | S_IROTH)
            os.chmod(f"./data/{name}/{name}.atlas", S_IREAD | S_IRGRP | S_IROTH)
        except Exception:
            pass

    return errors, warnings


def scripts_recopy(dir):
    for folder in dir:
        for root, dirs, files in os.walk(folder):
            for file in files:
                if file.endswith(".lua"):
                    copy_script(file=root + '/' + file)


def copy_script(file):
    if file.endswith(".lua"):
        command = [LUA, '-p', file]
        p = subprocess.Popen(command, stdout=subprocess.PIPE,
                             stdin=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = p.communicate()[0]
        if p.returncode != 0:
            print(colored(output.decode(), 'red'))
            # exit(p.returncode)  # Do not exit on lua errors

        name = Path(file).name
        Path("./data/scripts").mkdir(parents=True, exist_ok=True)

        if set_read_only:
            try:
                os.chmod(f'./data/scripts/{name}',
                         S_IREAD | S_IRGRP | S_IROTH | S_IWUSR)
            except Exception:
                pass
        shutil.copyfile(file, f'./data/scripts/{name}')
        if set_read_only:
            os.chmod(f'./data/scripts/{name}', S_IREAD | S_IRGRP | S_IROTH)


def copy_folder(src, des):
    for root, dirs, files in os.walk(src):
        for file in files:
            copy_file(src=root + '/' + file, des=des)


def copy_file(src, des):
    src = src.replace('\\', '/')
    name = Path(src).name
    Path(des).mkdir(parents=True, exist_ok=True)

    if set_read_only:
        try:
            os.chmod(f'{des}/{name}', S_IREAD | S_IRGRP | S_IROTH | S_IWUSR)
        except Exception:
            pass
    shutil.copyfile(src, f'{des}/{name}')
    if set_read_only:
        os.chmod(f'{des}/{name}', S_IREAD | S_IRGRP | S_IROTH)


def on_created(event):
    print(colored(f"{event.src_path} has been created!", 'green'))


def on_deleted(event):
    if event.src_path.endswith(".png"):
        return
    print(colored(f"{event.src_path} was deleted!", 'red'))


def on_data_src_modified(event):
    time.sleep(.5)
    print(
        colored(f"data-src file '{event.src_path}' has been modified", 'green'))
    if event.src_path.endswith(".spine"):
        (errors, warnings) = spine_export(event.src_path)
        if len(errors) > 0 or len(warnings) > 0:
            printErrorsAndWarnings(event.src_path, errors, warnings)
        convert_png_to_webp()
    if event.src_path.endswith(".lua"):
        copy_script(event.src_path)
    if event.src_path.endswith(".json") and 'scenes' in event.src_path:
        file = Path(event.src_path).name
        copy_file(f"./data-src/scenes/{file}", "./data/scenes")
    if event.src_path.endswith(".json") and 'config' in event.src_path:
        file = Path(event.src_path).name
        copy_file(f"./data-src/config/{file}", "./data/config")
    if event.src_path.endswith(".schnack") and 'dialog' in event.src_path:
        file = Path(event.src_path).name
        copy_file(f"./data-src/dialog/{file}", "./data/dialog")


def on_moved(event):
    print(colored(
        f"ok ok ok, someone moved {event.src_path} to {event.dest_path}", 'green'))


if __name__ == "__main__":
    freeze_support()
    print(colored("Start convert", 'green'))
    spine_reexport(["./data-src"])
    scripts_recopy(["./data-src/scripts/"])
    copy_folder("./data-src/config", "./data/config")
    copy_folder("./data-src/fonts", "./data/fonts")
    copy_folder("./data-src/scenes", "./data/scenes")
    copy_folder("./data-src/audio", "./data/audio")
    copy_folder("./data-src/icons", "./data/icons")
    copy_folder("./data-src/dialog", "./data/dialog")
    rhubarb_reexport()
    convert_png_to_webp()
    print(colored("Convert sucess", 'green'))
    patterns_src = ["*.spine", "*.lua", "*.json", "*.schnack"]
    ignore_patterns = None
    ignore_directories = False
    case_sensitive = True

    go_recursively = True
    data_src_event_handler = PatternMatchingEventHandler(
        patterns_src, ignore_patterns, ignore_directories, case_sensitive)
    data_src_event_handler.on_created = on_created
    data_src_event_handler.on_deleted = on_deleted
    data_src_event_handler.on_modified = on_data_src_modified
    data_src_event_handler.on_moved = on_moved

    path = "./data-src/"
    data_src_observer = Observer()
    data_src_observer.schedule(
        data_src_event_handler, path, recursive=go_recursively)
    data_src_observer.start()

    print(colored("Observer Started", 'green'))

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        data_src_observer.stop()
        data_src_observer.join()
