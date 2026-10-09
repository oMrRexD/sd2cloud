"""Checks that the texts on screen (messages.def) fit where they are drawn, in both languages.

Measures like the app: the same font (Varela Round) at the same sizes, plus what the app's emboldening adds to each
letter (font.c: the advance grows by the "bold" amount). Where each text goes and how wide that place is follows
src/app and src/ui/look.c (a 640 px screen; the frame's lines go from x = 36 to 604).

usage: python tools/check_messages.py [--device NAME]     (exits with an error if a text doesn't fit its place)

The texts name the sd2psx; on another device the app puts that device's name in its place (i18n_device).
--device "MemCard PRO2" checks them as they are shown there.
"""
import pathlib
import re
import sys

from PIL import ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
TTF = str(ROOT / "third_party/varelaround/VarelaRound-Regular.ttf")
# the fonts of ui.c: pixels, emboldening in 1/64 pixel
SPEC = {"text": (18, 40), "small": (15, 24), "title": (26, 52), "browser": (23, 56)}
fonts = {k: ImageFont.truetype(TTF, px) for k, (px, bold) in SPEC.items()}


def measure(font, s):
    """as font.c walks a text: the pen in 1/64 pixel, each letter's advance plus the bold, and a word starting on a
    whole pixel (the pen is rounded up after each space)"""
    pen = 0
    for ch in s:
        pen += round(fonts[font].getlength(ch) * 64) + SPEC[font][1]
        if ch == " ":
            pen = (pen + 63) // 64 * 64
    return (pen + 32) >> 6


def count_lines(font, s, width):
    """how many lines ui_paragraph uses (wraps at words and at line breaks)"""
    n = 0
    for part in s.split("\n"):
        line = ""
        for word in part.split():
            test = f"{line} {word}" if line else word
            if line and measure(font, test) > width:
                n, line = n + 1, word
            else:
                line = test
        n += 1 if line else 0
    return n


LONG_CARD = "SLUS-21065-1"          # a long card name, for the texts that show one
LONG_DATE = "02/10/2026 21:10"
LONG_SAVE = "Shin Megami Tensei: Digital Devil Saga 2"   # a long icon.sys title, for the questions about a save
LONG_FILE = "BASLUS-21065SAVEDATA0001.psu"   # a long name of an exported save
LONG_TEMPLATE = "My Network Settings 2026"   # a template's name, as long as it can be


def fill(s, id_):
    """%d, %lld and %s with the worst case"""
    if "%s" in s and id_ in ("T_CONFIRM_COPY", "T_CONFIRM_MOVE", "T_DELETE_ASK", "T_CONFIRM_CLOUD", "T_CONFIRM_IMPORT",
                             "T_CONFIRM_EXPORT", "T_TPL_REMOVE_ASK"):
        s = s.replace("%s", LONG_SAVE, 1).replace("%s", LONG_CARD)
    if "%s" in s and id_ in ("T_TPL_CREATE_ASK", "T_TPL_CREATED", "T_TPL_DELETE_ASK", "T_TPL_ADD_ASK",
                             "T_TPL_MAIN_NOW", "T_TPL_GAMES_ASK", "T_TPL_UPDATE_ASK", "T_TPL_PICK_ASK"):
        s = s.replace("%s", LONG_TEMPLATE, 1).replace("%s", LONG_CARD)
    if id_ == "T_TPL_SUMMARY":
        s = s.replace("%d", "32", 1)   # saves, then KB
    if "%s" in s:
        if id_ in ("T_HIST_TITLE", "T_RESTORE_TITLE", "T_RESTORE_IN_USE", "T_RESTORING", "T_CARD_IN_USE", "T_DONE_COPY",
                   "T_DONE_MOVE", "T_DELETE_TEXT", "T_ERR_EXISTS", "T_ERR_FULL", "T_ERR_MC_CHECK", "T_DONE_IMPORT",
                   "T_SWITCH_ASK", "T_INSERT_DONE", "T_INSERT_FAILED", "T_SWITCH_BACK_FAILED", "T_INSTALL_REPLACE_ASK",
                   "T_INSTALLING", "T_SKIP_ASK", "T_MOVE_SAVES", "T_MOVE_SAVES_ASK", "T_MOVE_SAVES_NOTE"):
            s = s.replace("%s", LONG_CARD)
        elif id_ in ("T_CARD_LAST", "T_RESTORE_FROM"):
            s = s.replace("%s", LONG_DATE)
        elif id_ in ("T_EXPORT_FILE", "T_EXPORT_REPLACE", "T_DONE_EXPORT"):
            s = s.replace("%s", LONG_FILE)
        else:
            s = s.replace("%s", "v10.10" if "SD2Cloud" in s else "999 MB")
    if id_ in ("T_HELPER_SPACE", "T_HELPER_SPACE_NEED", "T_HELPER_FULL", "T_FREE_KB", "T_SAVE_KB", "T_TPL_SUMMARY"):
        s = s.replace("%d", "8192")   # KB of a memory card
    return s.replace("%lld", "99999999").replace("%d", "99")


DIALOG, WIDE = 440 - 56, 520 - 56   # the inside of the message boxes (normal and wide)
# where each text is drawn: (font, width, lines allowed). 1 line = must not overflow; paragraphs wrap by
# themselves, but more lines than the limit clutter the box.
PLACE = {
    # dialog titles (they wrap, but should take one line)
    "T_SEARCHING": ("text", DIALOG, 1), "T_LOGIN_OK": ("text", DIALOG, 1),
    "T_LOGIN_ERROR": ("text", DIALOG, 1), "T_FIRST_TITLE": ("text", DIALOG, 1), "T_SUMMARY_OK": ("text", WIDE, 1),
    "T_SUMMARY_PARTIAL": ("text", WIDE, 1), "T_SUMMARY_FAILED": ("text", WIDE, 1),
    "T_CANCEL_TITLE": ("text", DIALOG, 1), "T_RESTORE_TITLE": ("text", WIDE, 1), "T_RESTORING": ("text", DIALOG, 1),
    "T_RESTORE_OK": ("text", DIALOG, 1), "T_RESTORE_FAILED": ("text", DIALOG, 1), "T_RESTORE_CANCEL_TITLE": ("text", DIALOG, 1),
    "T_IGR_DONE": ("text", DIALOG, 1), "T_OPTIONS": ("text", 300, 1),
    # the sign-in box: the left column (the QR is on the right)
    "T_LOGIN_TITLE": ("text", 298, 1), "T_LOGIN_OPEN": ("text", 298, 1), "T_LOGIN_ENTER": ("text", 298, 1),
    "T_LOGIN_EXPIRES": ("small", 298, 1), "T_LOGIN_QR": ("small", 148, 1),
    # under the big card: the status on the left of the counter, the last backup and the history's marker
    "T_ST_NEW": ("text", 200, 1), "T_ST_CHANGED": ("text", 200, 1), "T_ST_UP_TO_DATE": ("text", 200, 1),
    "T_ST_ERROR": ("text", 200, 1), "T_ST_SKIPPED": ("text", 200, 1), "T_CARD_LAST": ("small", 244, 1),
    "T_HIST_SAME": ("small", 136, 1), "T_HIST_TITLE": ("small", 244, 1),
    # the backup screen (the column on the right of the icon)
    "T_BACKING_UP": ("text", 296, 1), "T_UPLOADING": ("small", 120, 1), "T_CANCELLING": ("text", 296, 1),
    "T_WORK_CANCEL_TITLE": ("text", DIALOG, 1),
    # the saves screen
    "T_FREE_KB": ("text", 300, 1), "T_CARD_EMPTY": ("browser", 540, 1),
    # inside the dialogs
    "T_HELPER_ABOUT": ("text", WIDE, 2), "T_HELPER_WHERE": ("text", WIDE, 1), "T_HELPER_SPACE": ("text", WIDE, 1),
    "T_HELPER_SPACE_NEED": ("text", WIDE, 1), "T_HELPER_OK": ("text", WIDE, 1),
    "T_HELPER_UNINSTALL_ASK": ("text", DIALOG, 1), "T_HELPER_UNINSTALL_TEXT": ("text", DIALOG, 3),
    "T_HELPER_ELSEWHERE": ("text", WIDE, 4), "T_HELPER_MISSING": ("text", WIDE, 2), "T_HELPER_USB": ("text", WIDE, 3),
    "T_HELPER_OLD_PATH": ("small", WIDE, 2), "T_NEWCARD_NOTE": ("text", DIALOG, 2),
    "T_NEWCARD_MAKING": ("text", DIALOG, 1), "T_NEWCARD_FAILED": ("text", DIALOG, 2),
    "T_HELPER_AUTOBOOT": ("small", WIDE, 1), "T_FIRST_QUESTION": ("text", DIALOG, 2), "T_FIRST_AFTER": ("small", DIALOG, 1),
    "T_CANCEL_TEXT": ("text", DIALOG, 2), "T_RESTORE_FROM": ("text", WIDE, 1), "T_RESTORE_TEXT": ("text", WIDE, 2),
    "T_RESTORE_SAVE_FIRST": ("small", WIDE, 1), "T_RESTORE_IN_USE": ("small", WIDE, 2), "T_RESTORE_UNKNOWN": ("small", WIDE, 2),
    "T_RESTORE_STEP_DOWNLOAD": ("text", DIALOG, 1), "T_RESTORE_STEP_WRITE": ("text", DIALOG, 1),
    "T_RESTORE_CANCEL_TEXT": ("text", DIALOG, 1), "T_RETURNING": ("small", WIDE, 1), "T_HIST_LOADING": ("text", DIALOG, 1),
    "T_HIST_EMPTY": ("text", 150, 3), "T_NO_CARDS": ("text", 150, 5), "T_IGR_NOTHING": ("text", DIALOG, 2),
    # the settings (START): labels from x = 80, values up to x = 560 (cut with "..." past 200 px). The IGR helper's
    # and the automatic sync's labels are also the titles of their dialogs, which are wider. The label of the backups
    # kept has more room: its values are a number or "No limit"
    "T_SET_SYNC_ALL": ("text", 270, 1), "T_HELPER_TITLE": ("text", 270, 1), "T_SET_IGR_RETURN": ("text", 270, 1),
    "T_IGR_TITLE": ("text", 270, 1), "T_SYNC_ON": ("text", 200, 1), "T_SYNC_OFF": ("text", 200, 1),
    "T_SET_LANGUAGE": ("text", 270, 1), "T_SET_KEEP": ("text", 290, 1), "T_SET_FORMAT": ("text", 270, 1),
    "T_SET_UPDATES": ("text", 270, 1), "T_SET_ACCOUNT": ("text", 270, 1), "T_SET_ABOUT": ("text", 270, 1),
    "T_MENU_UPDATE": ("text", 440, 1), "T_PENDING_N": ("text", 200, 1), "T_NONE_PENDING": ("text", 200, 1), "T_UPDATE_AVAILABLE": ("text", 200, 1),
    "T_HELPER_NOT_INSTALLED": ("text", 200, 1), "T_ACCOUNT_OFF": ("text", 200, 1),
    "T_SYNC_NOW": ("text", 320, 1), "T_EXIT_TO": ("text", 330, 1), "T_EXIT_BROWSER": ("text", 320, 1), "T_ALL_SYNCED": ("text", DIALOG, 1), "T_AUTO_ASK": ("text", WIDE, 3), "T_AUTO_WHERE": ("small", WIDE, 2),
    "T_AUTO_OPL": ("text", WIDE, 1), "T_ABOUT_CREDITS": ("small", WIDE, 3), "T_ABOUT_LICENSES": ("small", WIDE, 1), "T_AUTO_NOTE": ("small", WIDE, 1), "T_AUTO_DONE": ("text", WIDE, 1), "T_CONNECT_HINT": ("small", DIALOG, 2),
    # the box with a list (a card's options, the exit menu): title and items centered in 380 px; an item wider than
    # 320 px is cut with "..." (main.c choose)
    "T_RESTORE_BACKUP": ("text", 320, 1), "T_COPY_DEVICE": ("text", 320, 1), "T_HELPER_REINSTALL": ("text", 320, 1), "T_HELPER_UNINSTALL": ("text", 320, 1),
    "T_UPDATE": ("text", 320, 1), "T_INSERT": ("text", 320, 1), "T_INSERT_PREVIEW": ("text", 320, 1),
    # moving the sd2psx to another card (a wide box, to say it all in three lines); what a card that it wasn't moved
    # to needs, under that
    "T_SWITCH_ASK": ("text", WIDE, 3), "T_SWITCHING": ("text", DIALOG, 1),
    "T_INSERT_DONE": ("text", DIALOG, 2), "T_INSERT_FAILED": ("text", DIALOG, 2),
    "T_INSERT_NEEDS_BOOT": ("small", DIALOG, 2), "T_INSERT_NEEDS_GAMEID": ("small", DIALOG, 2),
    "T_INSERT_BOOT_ASK": ("text", DIALOG, 2),
    # "Copy to" / "Move to": the title over the destination card, and what goes under it
    "T_COPY_TO": ("text", 200, 1), "T_MOVE_TO": ("text", 200, 1), "T_NO_ROOM": ("small", 200, 1),
    # a save's page: the column on the right, centered on x = 452
    "T_COPY": ("browser", 304, 1), "T_MOVE": ("browser", 304, 1), "T_DELETE": ("browser", 304, 1),
    "T_START_APP": ("browser", 304, 1), "T_EXIT_FILES": ("text", 320, 1),
    "T_TO_CLOUD": ("browser", 304, 1), "T_SAVE_KB": ("text", 304, 1),
    # the tabs over the list of cards (FONT_SMALL, the four side by side, centered on the 208 px list)
    "T_TAB_CARDS": ("small", 70, 1), "T_TAB_GAMES": ("small", 60, 1), "T_TAB_BOOT": ("small", 50, 1),
    "T_TAB_FILES": ("small", 70, 1),
    # "All saves", the first row of the Games group and the title over the big card; what goes under that card, and
    # under the title of the screen it opens
    "T_ALL_SAVES": ("text", 200, 1), "T_GAME_CARDS_N": ("text", 200, 1), "T_SAVES_COUNT": ("text", 300, 1),
    # the Files group: the devices in the list, what the group is for on the right, a folder that has nothing to show
    "T_DEV_SD": ("text", 172, 1), "T_DEV_USB": ("text", 172, 1), "T_FILES_HINT": ("small", 212, 5),
    "T_DIR_ERROR": ("text", 500, 1), "T_DIR_EMPTY": ("text", 500, 1),
    # a .psu file's page and the card pickers over it; the questions and answers of importing and exporting
    "T_IMPORT": ("browser", 304, 1), "T_IMPORT_TO": ("text", 200, 1),
    "T_CONFIRM_IMPORT": ("text", DIALOG, 3), "T_CONFIRM_EXPORT": ("text", DIALOG, 3), "T_EXPORT_FILE": ("text", DIALOG, 2),
    "T_DONE_IMPORT": ("text", DIALOG, 1), "T_DONE_EXPORT": ("text", DIALOG, 2), "T_LOADING": ("text", DIALOG, 1),
    "T_USB_SEARCHING": ("text", DIALOG, 1), "T_WORKING_IMPORT": ("text", DIALOG, 1), "T_WORKING_EXPORT": ("text", DIALOG, 1),
    "T_DELETE_ASK": ("text", DIALOG, 3), "T_CONFIRM_COPY": ("text", DIALOG, 3),
    "T_CONFIRM_MOVE": ("text", DIALOG, 3), "T_CONFIRM_CLOUD": ("text", DIALOG, 3), "T_CLOUD_DONE": ("text", DIALOG, 1), "T_DONE_DELETE": ("text", DIALOG, 1),
    # installing a card file: "New card" at the top of each list of cards and the title over the big card; the
    # questions (a wide box) and the bar's box; the list of games the file may be of
    "T_NEW_CARD": ("text", 172, 1), "T_INSTALL_TO": ("text", 200, 1), "T_INSTALL_NEW_ASK": ("text", WIDE, 1),
    "T_INSTALL_REPLACE_ASK": ("text", WIDE, 1), "T_INSTALL_REPLACE_TEXT": ("text", WIDE, 2),
    "T_INSTALL_BOOT_WARN": ("text", WIDE, 2), "T_INSTALL_NO_GAME": ("text", DIALOG, 4),
    "T_INSTALL_RAISE_ASK": ("text", WIDE, 3),
    "T_INSTALL_WHICH_GAME": ("text", 320, 1), "T_INSTALLING": ("text", DIALOG, 1), "T_INSTALL_OK": ("text", DIALOG, 1),
    "T_INSTALL_FAILED": ("text", DIALOG, 1), "T_INSTALL_CANCEL_TITLE": ("text", DIALOG, 1),
    "T_INSTALL_CANCEL_TEXT": ("text", DIALOG, 1), "T_KEEP_ALL": ("text", 200, 1),
    # the tools and the templates: the boxes with a list (the tools, a template's options), the list of templates
    # and the title over the big card, the keyboard's title, the screen where saves are marked, a template open on
    # the screen of saves (what is under its name, and in the middle when it has none), a save's page there, and
    # the screen of a save on its way
    "T_TOOLS": ("text", 320, 1), "T_TEMPLATES": ("text", 200, 1),
    "T_TPL_ADD_SAVES": ("text", 320, 1), "T_TPL_RENAME": ("text", 320, 1), "T_TPL_DELETE": ("text", 320, 1),
    "T_TPL_NEW": ("text", 172, 1), "T_TPL_NAME": ("text", 400, 1), "T_TPL_MARK_TITLE": ("text", 200, 1),
    "T_MARKED_N": ("text", 200, 1), "T_TPL_SUMMARY": ("text", 244, 1), "T_TPL_EMPTY": ("browser", 540, 1),
    "T_TPL_REMOVE": ("browser", 304, 1), "T_TPL_COPYING": ("text", 296, 1),
    "T_TPL_APPLYING": ("text", 296, 1), "T_TPL_RENAMING": ("text", DIALOG, 1),
    "T_TPL_APPLY_MENU": ("text", 320, 1), "T_TPL_APPLY_TITLE": ("text", 400, 1), "T_TPL_MAKE_MAIN": ("text", 320, 1), "T_TPL_UNMAKE_MAIN": ("text", 320, 1),
    "T_TPL_MAIN": ("small", 100, 1), "T_TPL_CHECKING": ("text", DIALOG, 1), "T_TPL_WARN": ("text", WIDE, 2),
    "T_TPL_GAMES": ("text", 320, 1), "T_TPL_UPDATE": ("text", 320, 1), "T_TPL_AFTER_GAME": ("text", 236, 1),
    "T_TPL_AFTER_HINT": ("small", 212, 6), "T_TPL_ON": ("text", 200, 1), "T_TPL_OFF": ("text", 200, 1),
    "T_TPL_UPDATE_NOTE": ("text", DIALOG, 4), "T_TPL_ALL_CARDS": ("text", 320, 1), "T_TPL_PICK_CARDS": ("text", 200, 1),
    "T_TPL_NO_MAIN": ("small", 212, 1), "T_SYNC_DISABLE": ("text", 320, 1), "T_SYNC_ENABLE": ("text", 320, 1),
    "T_MOVE_SAVES": ("text", 320, 1), "T_CARD_UNUSED": ("small", 244, 1), "T_SKIP_ASK": ("text", WIDE, 1),
    "T_SKIP_TEXT": ("text", WIDE, 1), "T_MOVE_SAVES_ASK": ("text", DIALOG, 2), "T_MOVE_SAVES_NOTE": ("text", DIALOG, 2),
}
PARAGRAPH = ("text", DIALOG, 3)     # error messages and the rest: up to 3 lines in a box
# what only an sd2psx is ever told (being moved to another card, its boot cards, one more channel for a folder): not
# checked with another device's name
ONLY_SD2PSX = {"T_INSERT", "T_INSERT_NEEDS_BOOT", "T_INSERT_BOOT_ASK", "T_SWITCH_ASK", "T_SWITCH_FAILED",
               "T_SWITCH_BACK_FAILED", "T_INSTALL_RAISE_ASK", "T_INSTALL_BOOT_WARN"}

# the button legends at the bottom: (texts, how many of the last ones stand apart at the right edge). They go from
# the right edge (x = 596) toward the left and must not pass x = 40
LEGENDS = [
    (("T_MENU_EXIT", "T_OPEN", "T_OPTIONS", "T_TOOLS", "T_SETTINGS"), 2), (("T_BACK", "T_OPEN", "T_SYNC"), False),
    (("T_BACK", "T_DELETE"), False), (("T_BACK", "T_SELECT"), False), (("T_BACK", "T_RESTORE"), False),
    (("T_CANCEL_NO", "T_CANCEL_YES"), False), (("T_LATER", "T_YES"), False),
    (("T_BACK", "T_HELPER_INSTALL"), False), (("T_CANCEL",), False), (("T_BACK", "T_SYNC"), False),
    (("T_BACK", "T_LOGOUT_YES"), False), (("T_BACK", "T_HELPER_UNINSTALL"), False), (("T_LATER", "T_AUTO_ON"), False),
    (("T_FINISH",), False), (("T_LATER", "T_CONNECT"), False), (("T_LATER", "T_NEVER_ASK", "T_CONNECT"), False),
    (("T_MENU_EXIT", "T_OPEN", "T_TOOLS", "T_SETTINGS"), 2), (("T_BACK", "T_OPEN", "T_INSTALL_SAVE"), False),
    (("T_BACK", "T_OPEN", "T_EXPORT"), False),
    (("T_BACK", "T_EXPORT"), False), (("T_BACK", "T_REPLACE"), False), (("T_BACK", "T_IMPORT_YES"), False),
    (("T_BACK", "T_CONTINUE"), False), (("T_BACK", "T_INSERT_YES"), False),
    (("T_BACK", "T_OPEN", "T_OPTIONS"), False), (("T_BACK", "T_OPEN", "T_INSTALL"), False),
    (("T_BACK", "T_REPLACE", "T_KEEP_BOTH"), False), (("T_BACK", "T_OPEN"), False), (("T_BACK", "T_INSTALL_YES"), False),
    (("T_BACK", "T_RUN"), False), (("T_EXIT_BROWSER",), False),
    # the keyboard, the screens where saves are marked, and the templates' questions
    (("T_BACK", "T_KB_TYPE", "T_KB_ERASE", "T_FINISH"), False), (("T_BACK", "T_MARK", "T_FINISH"), False),
    (("T_BACK", "T_OPEN", "T_FINISH"), False), (("T_BACK", "T_MARK_DROP_YES"), False), (("T_BACK", "T_TPL_CREATE"), False),
    (("T_BACK", "T_TPL_REMOVE_YES"), False), (("T_BACK", "T_TPL_APPLY"), False), (("T_BACK", "T_TPL_ADD"), False),
    (("T_BACK", "T_REPLACE", "T_TPL_KEEP"), False), (("T_LATER", "T_TPL_APPLY", "T_TPL_NO_WARN"), False),
    (("T_BACK", "T_MARK"), False), (("T_BACK", "T_TPL_UPDATE_YES"), False), (("T_BACK", "T_TPL_CHANGE"), False),
    (("T_BACK", "T_MARK", "T_TPL_APPLY"), False), (("T_BACK", "T_OPEN", "T_OPTIONS"), False),
    (("T_CANCEL", "T_SKIP"), False), (("T_CANCEL_NO", "T_SKIP", "T_SYNC_DISABLE"), False), (("T_BACK", "T_MOVE"), False),
]


def legend_width(texts, apart):
    """each button is a 20 px symbol, 6 px and its text; the last apart of them stand apart from the others, by a
    wider gap, and always 18 px from each other; five of them stand closer together (look_legend)"""
    gap, wide = (18, 34) if len(texts) > 4 else (26, 70)
    inner = apart - 1 if apart else 0
    w = sum(measure("text", t) + 26 for t in texts) + 18 * inner + gap * (len(texts) - 1 - inner)
    return w + (wide - gap if apart and len(texts) > apart else 0)


def main():
    txt = (ROOT / "src" / "ui" / "messages.def").read_text(encoding="utf-8")
    items = re.findall(r'^X\((\w+),\s*"((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)"\)', txt, re.M)
    texts = {id_: (en.replace('\\"', '"'), pt.replace('\\"', '"')) for id_, en, pt in items}
    if "--device" in sys.argv:
        name = sys.argv[sys.argv.index("--device") + 1]
        texts = {id_: (en.replace("sd2psx", name), pt.replace("sd2psx", name)) for id_, (en, pt) in texts.items()
                 if id_ not in ONLY_SD2PSX}
    bad = 0
    for id_, (en, pt) in texts.items():
        font, width, max_lines = PLACE.get(id_, PARAGRAPH)
        for lang, s in (("en", en), ("pt", pt)):
            s = fill(s, id_)
            w = measure(font, s)
            n = 1 if w <= width else count_lines(font, s, width)
            if n > max_lines:
                bad += 1
                print(f"  DOESN'T FIT  {id_} [{lang}] {w} px / {width} px ({n} lines, max {max_lines}): {s}")
    for ids, apart in LEGENDS:
        for k, lang in enumerate(("en", "pt")):
            w = legend_width([texts[i][k] for i in ids], apart)
            if w > 596 - 40:
                bad += 1
                print(f"  LEGEND TOO WIDE [{lang}] {w} px: {' / '.join(texts[i][k] for i in ids)}")
    print(f"{len(items)} texts x 2 languages and {len(LEGENDS)} legends checked; {bad} problem(s)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
