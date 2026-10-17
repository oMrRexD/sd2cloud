"""Checks the texts on screen, one file per language in lang/ (en.txt is the original): that each file has the ids of
src/ui/messages.def and no others (English and Portuguese all of them; another language may leave some in English),
that %s and %d are the original's, that the font has every letter they use, and that each text fits where it is drawn.

Measures like the app: the same font (assets/font.ttf: Varela Round, with Cyrillic letters) at the same sizes, plus
what the app's emboldening adds to each letter (font.c: the advance grows by the "bold" amount). Where each text goes and how wide that place is follows
src/app and src/ui/look.c (a 640 px screen; the frame's lines go from x = 36 to 604).

usage: python tools/check_messages.py [--device NAME] [--sync]     (exits with an error on a problem; needs Pillow and
fontTools)

--sync writes the files of lang/ again in the order of messages.def, each text with a comment above it saying what it
is (and, in a translation, the English text): run it after adding a text to en.txt, so that the other languages get it
to translate (empty, which shows in English until it is). Each file's own comments at its top are kept.

The texts name the sd2psx; on another device the app puts that device's name in its place (i18n_device).
--device "MemCard PRO2" checks them as they are shown there.
"""
import pathlib
import re
import sys

import gzip
import io

from fontTools.ttLib import TTFont
from PIL import ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
TTF = str(ROOT / "assets/font.ttf")
JA = gzip.decompress((ROOT / "assets/font-ja.ttf.gz").read_bytes())   # the Japanese characters' font (font.c)
BASE_LETTERS = set(TTFont(TTF).getBestCmap())
LETTERS = BASE_LETTERS | set(TTFont(io.BytesIO(JA)).getBestCmap())   # what the fonts have
# the fonts of ui.c: pixels, emboldening in 1/64 pixel
SPEC = {"text": (18, 40), "small": (15, 24), "title": (26, 52), "browser": (23, 56)}
fonts = {k: ImageFont.truetype(TTF, px) for k, (px, bold) in SPEC.items()}
jaFonts = {k: ImageFont.truetype(io.BytesIO(JA), px) for k, (px, bold) in SPEC.items()}


def measure(font, s):
    """as font.c walks a text: the pen in 1/64 pixel, each letter's advance plus the bold, and a word starting on a
    whole pixel (the pen is rounded up after each space)"""
    pen = 0
    for ch in s:
        pen += round((fonts if ord(ch) in BASE_LETTERS else jaFonts)[font].getlength(ch) * 64) + SPEC[font][1]
        if ch == " ":
            pen = (pen + 63) // 64 * 64
    return (pen + 32) >> 6


# Japanese: its words aren't apart, so a line may end between two of its characters, but doesn't start with these
# nor end with those (ui.c, word_length)
NO_START = set("、。，．・：；？！ー」』）〕］｝〉》】っゃゅょぁぃぅぇぉゎッャュョァィゥェォヮヵヶ゛゜々〟")
NO_END = set("「『（〔［｛〈《【〝")


def japanese(c):
    return 0x3000 <= ord(c) <= 0x9FFF or 0xFF00 <= ord(c) <= 0xFFEF


def words(part):
    """the words of a line of text as ui.c lays them out, each with whether a space comes before it"""
    out, i, apart = [], 0, False
    while i < len(part):
        if part[i] == " ":
            i, apart = i + 1, True
            continue
        j = i + 1
        while j < len(part) and part[j] != " " and not (
                (japanese(part[j - 1]) or japanese(part[j])) and part[j - 1] not in NO_END and part[j] not in NO_START):
            j += 1
        out.append((part[i:j], apart))
        i, apart = j, False
    return out


def count_lines(font, s, width):
    """how many lines ui_paragraph uses (wraps at words and at line breaks)"""
    n = 0
    for part in s.split("\n"):
        line = ""
        for word, apart in words(part):
            test = f"{line}{' ' if line and apart else ''}{word}"
            if line and measure(font, test) > width:
                # (inside Japanese, back to the line's last ideographic space when what follows it fits: ui.c)
                rest = line[line.rindex("\u3000") + 1:] + word if not apart and "\u3000" in line[1:] else None
                n, line = n + 1, rest if rest is not None and measure(font, rest) <= width else word
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
                   "T_INSTALLING", "T_SKIP_ASK", "T_MOVE_SAVES", "T_MOVE_SAVES_ASK", "T_MOVE_SAVES_NOTE",
                   "T_CONFIRM_COPY_N"):
            s = s.replace("%s", LONG_CARD)
        elif id_ in ("T_CARD_LAST", "T_RESTORE_FROM"):
            s = s.replace("%s", LONG_DATE)
        elif id_ in ("T_EXPORT_FILE", "T_EXPORT_REPLACE", "T_DONE_EXPORT"):
            s = s.replace("%s", LONG_FILE)
        else:
            s = s.replace("%s", "v10.10" if "SD2Cloud" in s else "999 MB")
    if id_ in ("T_HELPER_SPACE", "T_HELPER_SPACE_NEED", "T_HELPER_FULL", "T_FREE_KB", "T_SAVE_KB", "T_TPL_SUMMARY"):
        s = s.replace("%d", "8192")   # KB of a memory card
    if id_ == "T_HELPER_DROPPED_HINT":
        s = s.replace("%d", "128")    # MB of a big memory card
    return s.replace("%lld", "99999999").replace("%d", "99")


# texts that go into another one, in the place of its %s (settings.c, helper_failed): measured there
INSIDE = {"T_HELPER_ERROR": "T_MC_ERROR", "T_HELPER_DROPPED": "T_MC_ERROR", "T_HELPER_DROPPED_HINT": "T_HELPER_CARD_MB"}

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
    "T_HELPER_ERROR": ("text", WIDE, 2), "T_HELPER_DROPPED": ("text", WIDE, 3), "T_HELPER_DROPPED_HINT": ("small", WIDE, 1),
    "T_HELPER_UNINSTALL_ASK": ("text", DIALOG, 1), "T_HELPER_UNINSTALL_TEXT": ("text", DIALOG, 3),
    "T_HELPER_MISSING": ("text", WIDE, 2),
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
    "T_SET_UPDATES": ("text", 270, 1), "T_SET_DEVICE": ("text", 270, 1), "T_SORT_BY": ("text", 140, 1), "T_REPEATED": ("text", 140, 1), "T_SORT_NEWEST": ("text", 170, 1), "T_SORT_OLDEST": ("text", 170, 1), "T_SORT_NAME": ("text", 170, 1), "T_REPEATED_HIDDEN": ("text", 170, 1), "T_REPEATED_SHOWN": ("text", 170, 1), "T_DEVICE_ASK": ("text", 320, 1), "T_SET_ACCOUNT": ("text", 270, 1), "T_SET_ABOUT": ("text", 270, 1),
    "T_MENU_UPDATE": ("text", 440, 1), "T_PENDING_N": ("text", 200, 1), "T_NONE_PENDING": ("text", 200, 1), "T_UPDATE_AVAILABLE": ("text", 200, 1),
    "T_HELPER_NOT_INSTALLED": ("text", 200, 1), "T_ACCOUNT_OFF": ("text", 200, 1),
    "T_SYNC_NOW": ("text", 320, 1), "T_EXIT_TO": ("text", 330, 1), "T_EXIT_BROWSER": ("text", 320, 1), "T_ALL_SYNCED": ("text", DIALOG, 1), "T_AUTO_ASK": ("text", WIDE, 3), "T_AUTO_WHERE": ("small", WIDE, 2),
    "T_AUTO_OPL": ("text", WIDE, 1), "T_ABOUT_CREDITS": ("small", WIDE, 2), "T_ABOUT_LICENSES": ("small", WIDE, 1), "T_AUTO_NOTE": ("small", WIDE, 1), "T_AUTO_DONE": ("text", WIDE, 1), "T_CONNECT_HINT": ("small", DIALOG, 2),
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
    # (the tabs, each on one line: the five together are measured below, as they share 300 px)
    "T_TAB_CARDS": ("small", 300, 1), "T_TAB_GAMES": ("small", 300, 1), "T_TAB_BOOT": ("small", 300, 1),
    "T_TAB_FILES": ("small", 300, 1),
    # "All saves", the first row of the Games group and the title over the big card; what goes under that card, and
    # under the title of the screen it opens
    "T_ALL_SAVES": ("text", 200, 1), "T_GAME_CARDS_N": ("text", 200, 1), "T_SAVES_COUNT": ("text", 300, 1),
    # the Files group: the devices in the list, what the group is for on the right, a folder that has nothing to show
    "T_DEV_SD": ("text", 172, 1), "T_DEV_USB": ("text", 172, 1), "T_DEV_HDD": ("text", 172, 1), "T_DEV_MX4SIO": ("text", 172, 1), "T_FILES_HINT": ("small", 212, 5),
    # the Memory Card: a row of the Files group (and a card's name in the questions), and what it is for on the right
    "T_MEMCARD": ("text", 140, 1), "T_MEMCARD_HINT": ("small", 212, 5), "T_MEMCARD_HINT_ALONE": ("small", 212, 5), "T_HELPER_ALONE_USB": ("text", WIDE, 4),
    "T_HELPER_ALONE_NONE": ("text", WIDE, 5),   # (140: also on the picture of a card)
    "T_MEMCARD_READING": ("text", DIALOG, 1), "T_MEMCARD_READ_FAILED": ("text", DIALOG, 3),
    "T_CARD_FILE_READ_ONLY": ("text", DIALOG, 4),
    "T_DIR_ERROR": ("text", 500, 1), "T_DIR_EMPTY": ("text", 500, 1),
    # a .psu file's page and the card pickers over it; the questions and answers of importing and exporting
    "T_IMPORT": ("browser", 304, 1), "T_IMPORT_TO": ("text", 200, 1),
    "T_CONFIRM_IMPORT": ("text", DIALOG, 3), "T_CONFIRM_EXPORT": ("text", DIALOG, 3), "T_EXPORT_FILE": ("text", DIALOG, 2),
    "T_DONE_IMPORT": ("text", DIALOG, 1), "T_DONE_EXPORT": ("text", DIALOG, 2), "T_LOADING": ("text", DIALOG, 1),
    "T_USB_SEARCHING": ("text", DIALOG, 1), "T_WORKING_IMPORT": ("text", DIALOG, 1), "T_WORKING_EXPORT": ("text", DIALOG, 1),
    "T_DELETE_ASK": ("text", DIALOG, 3), "T_CONFIRM_COPY": ("text", DIALOG, 3), "T_CONFIRM_COPY_N": ("text", DIALOG, 2),
    "T_CONFIRM_EXPORT_N": ("text", DIALOG, 2),
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
    "T_TPL_APPLY_MENU": ("text", 320, 1), "T_TPL_APPLY_TITLE": ("text", 400, 1), "T_TPL_MAKE_MAIN": ("text", 320, 1), "T_TPL_UNMAKE_MAIN": ("text", 320, 1), "T_TPL_WARN_AGAIN": ("text", 320, 1),
    "T_TPL_MAIN": ("small", 100, 1), "T_TPL_CHECKING": ("text", DIALOG, 1), "T_TPL_WARN": ("text", WIDE, 2),
    "T_TPL_GAMES": ("text", 320, 1), "T_TPL_UPDATE": ("text", 320, 1), "T_TPL_AFTER_GAME": ("text", 236, 1),
    "T_TPL_AFTER_HINT": ("small", 212, 6), "T_TPL_ON": ("text", 200, 1), "T_TPL_OFF": ("text", 200, 1),
    "T_TPL_UPDATE_NOTE": ("text", DIALOG, 4), "T_TPL_ALL_CARDS": ("text", 320, 1), "T_TPL_PICK_CARDS": ("text", 200, 1),
    "T_TPL_NO_MAIN": ("small", 212, 1), "T_HELPER_UPDATING": ("text", DIALOG, 1), "T_SYNC_DISABLE": ("text", 320, 1), "T_SYNC_ENABLE": ("text", 320, 1),
    "T_MOVE_SAVES": ("text", 320, 1), "T_CARD_UNUSED": ("small", 244, 1), "T_SKIP_ASK": ("text", WIDE, 1),
    "T_SKIP_TEXT": ("text", WIDE, 1), "T_MOVE_SAVES_ASK": ("text", DIALOG, 2), "T_MOVE_SAVES_NOTE": ("text", DIALOG, 2),
    # the list of a sync's cards (synclist.c): its title on the left and the count on the right, over the rows; what
    # became of each card, on the right of its row; what can be done to a card, in the box with a list
    "T_SYNC_TITLE": ("text", 260, 1), "T_SYNC_CARD_N": ("text", 200, 1), "T_SY_WAITING": ("small", 220, 1),
    "T_SY_SENDING": ("small", 220, 1), "T_SY_SENT": ("small", 220, 1), "T_SY_FAILED": ("small", 220, 1),
    "T_SY_SKIPPED": ("small", 220, 1), "T_SY_RESOLVED": ("small", 220, 1), "T_SY_OFF": ("small", 220, 1),
    "T_MARK_RESOLVED": ("text", 320, 1),
    # a card's size: the wide box with a list (dialog.c, choose_noted), its title and the items in a 250 px column;
    # beside them, in the small letters, what the saves take, what the card has left, and what a size asks of the
    # microSD (past 8 MB) or of the game (under it, made by the sd2psx)
    "T_SIZE_TITLE": ("text", 480, 1), "T_SIZE_MB": ("text", 230, 1), "T_SIZE_RECOMMENDED": ("text", 230, 1),
    "T_SIZE_NEED": ("small", 234, 2), "T_SIZE_FREE": ("small", 234, 2), "T_SIZE_BIG": ("small", 234, 5),
    "T_SIZE_UNFORMATTED": ("small", 234, 6), "T_SET_CARD_SIZE": ("text", 270, 1),
    # a card deleted, or its whole folder (cardops.c): the items of its options; the question, in a wide box: the
    # title, what goes, and its backups in the small letters
    "T_DELETE_CARD": ("text", 320, 1), "T_DELETE_FOLDER": ("text", 320, 1), "T_DELETE_CARD_ASK": ("text", WIDE, 2),
    "T_DELETE_CARD_TEXT": ("text", WIDE, 3), "T_DELETE_BACKUP": ("small", WIDE, 2), "T_DELETE_NO_BACKUP": ("small", WIDE, 1),
    "T_DELETE_FOLDER_ASK": ("text", WIDE, 2), "T_DELETE_FOLDER_TEXT": ("text", WIDE, 3),
    "T_DELETE_FOLDER_NO_BACKUP": ("small", WIDE, 1), "T_DELETE_FOLDER_BACKUPS": ("small", WIDE, 2),
    "T_DELETING": ("text", DIALOG, 1), "T_DONE_DELETE_CARD": ("text", DIALOG, 1), "T_DONE_DELETE_FOLDER": ("text", DIALOG, 1),
    # a card made again of another size: its option, the box of sizes (title, an item), the question in a wide box,
    # what the work screen says on its way (as T_TPL_COPYING), and how it went
    "T_RESIZE": ("text", 320, 1), "T_RESIZE_TITLE": ("text", 480, 1), "T_SIZE_CURRENT": ("text", 230, 1),
    "T_RESIZE_ASK": ("text", WIDE, 2), "T_RESIZE_TEXT": ("text", WIDE, 4), "T_RESIZE_MAKING": ("text", 296, 1),
    "T_RESIZE_COPYING": ("text", 296, 1), "T_RESIZE_DONE": ("text", DIALOG, 2), "T_RESIZE_LEFT": ("text", DIALOG, 5),
    "T_RESIZE_ODD": ("text", DIALOG, 4), "T_RESIZE_FULL": ("text", DIALOG, 3),
    # renaming: the option, the keyboard's titles, the question of a folder in a wide box, how it went; a file of the
    # Files group deleted (its question, in a wide box)
    "T_RENAME": ("text", 320, 1), "T_CARD_NAME_TITLE": ("text", 400, 1), "T_FOLDER_NAME_TITLE": ("text", 400, 1),
    "T_FILE_NAME_TITLE": ("text", 400, 1), "T_RENAME_FOLDER_ASK": ("text", WIDE, 2), "T_RENAME_FOLDER_TEXT": ("text", WIDE, 3),
    "T_RENAMING": ("text", DIALOG, 1), "T_RENAME_DONE_FOLDER": ("text", DIALOG, 2), "T_DELETE_FILE_ASK": ("text", WIDE, 2),
    # a new card: under the big card of "New card"; the boxes with a list (where it goes, its format, the saves of a
    # folder that are on more than one card); the question, in a wide box; how it went; the row of the options of
    # "All saves" (no value on its right); the question of a folder marked on another card, in a wide box
    "T_CREATE_HINT": ("small", 212, 2), "T_CREATE_WHERE": ("text", 320, 1), "T_CREATE_NUMBERED": ("text", 320, 1),
    "T_CREATE_IN_GROUP": ("text", 320, 1), "T_CREATE_NEW_GROUP": ("text", 320, 1), "T_CREATE_FILE_SD": ("text", 320, 1),
    "T_CREATE_FILE_USB": ("text", 320, 1), "T_CREATE_FILE_HDD": ("text", 320, 1), "T_CREATE_FILE_MX4SIO": ("text", 320, 1), "T_CREATE_FORMAT": ("text", 320, 1), "T_CREATE_ASK": ("text", WIDE, 2),
    "T_CREATE_FILE_ASK": ("text", WIDE, 2), "T_CREATE_SAVES": ("text", WIDE, 1), "T_CREATE_DONE": ("text", DIALOG, 2),
    "T_EXPORT_ALL": ("text", 320, 1), "T_REPEATED_ASK": ("text", 320, 1), "T_REPEATED_NEWEST": ("text", 320, 1),
    "T_REPEATED_EACH": ("text", 320, 1), "T_MARK_SAME_ASK": ("text", WIDE, 2), "T_MARK_SAME_TEXT": ("text", WIDE, 2), "T_MARK_SAME_SELECTED": ("text", WIDE, 1),
    "T_MARK_SAME_THIS": ("text", WIDE, 1),
    # a card's saves by game (migrate.c): the option; the screen's title on the left; on the right of a game's row,
    # where its saves go; in the middle when there is none; under the list, how many go and stay; the box with a list
    # of what to do with the ones that are there already, saved differently; the questions, in a wide box; the screen
    # of the copy; how it went
    "T_MIGRATE": ("text", 320, 1), "T_MIGRATE_TITLE": ("text", 300, 1), "T_MIGRATE_OFF": ("small", 180, 1),
    "T_MIGRATE_NEW": ("small", 180, 1), "T_MIGRATE_NONE": ("text", 420, 3), "T_MIGRATE_SUMMARY": ("small", 430, 1),
    "T_MIGRATE_DIFFER": ("text", 320, 2), "T_MIGRATE_KEEP_GAME": ("text", 320, 1), "T_MIGRATE_REPLACE": ("text", 320, 1),
    "T_MIGRATE_NEWER": ("text", 320, 1), "T_MIGRATE_ASK": ("text", WIDE, 2), "T_MIGRATE_ASK_NEW": ("text", WIDE, 1),
    "T_MIGRATE_ASK_KEEP": ("small", WIDE, 2), "T_MIGRATE_THERE_N": ("text", DIALOG, 1),
    "T_MIGRATE_KEPT_N": ("text", DIALOG, 1), "T_VIEW_NOTE": ("text", 200, 1),
    # the VMCs' group: its tab; under a VMC's name, over its big card; the rename's keyboard and question
    "T_TAB_VMC": ("small", 300, 1), "T_VMC_GAMES": ("small", 200, 1), "T_VMC_NAME_TITLE": ("text", 320, 1),
    "T_RENAME_VMC_ASK": ("text", WIDE, 2), "T_NEW_VMC": ("text", 172, 1), "T_VMC_CREATE_HINT": ("small", 212, 3),
    "T_CREATE_VMC_ASK": ("text", WIDE, 2),
    # the Memory Card put back from a backup: what happens, under the question; how it went
    "T_MEMCARD_RESTORE_ASK": ("text", WIDE, 1), "T_MEMCARD_RESTORE_TEXT": ("text", WIDE, 4), "T_MEMCARD_RESTORED": ("text", DIALOG, 1),
    # "Check card": the option; what was found, in a wide box
    "T_CHECK_CARD": ("text", 320, 1), "T_CHECK_OK": ("text", WIDE, 1),
    "T_CHECK_PROBLEMS": ("text", WIDE, 1), "T_CHECK_CONTENTS": ("text", WIDE, 1), "T_CHECK_USED": ("text", WIDE, 1),
    "T_CHECK_CHAINS": ("text", WIDE, 1), "T_CHECK_CROSSED": ("text", WIDE, 1), "T_CHECK_LOST": ("text", WIDE, 1),
    "T_CHECK_ENTRIES": ("text", WIDE, 1), "T_CHECK_ADVICE": ("small", WIDE, 2),
    # the safety copies: the Tools' item and its list's title; the screen while one is made; when it can't be; the
    # note under the questions it is made after; restoring and deleting one
    "T_SAFETY_TITLE": ("text", 320, 1), "T_SAFETY_MAKING": ("text", 296, 1), "T_SAFETY_FAILED": ("text", WIDE, 2),
    "T_SAFETY_FAILED_TEXT": ("text", WIDE, 1), "T_SAFETY_NONE": ("text", 296, 2), "T_SAFETY_NOTE": ("small", WIDE, 2),
    "T_SAFETY_REPLACE_ASK": ("text", WIDE, 2), "T_SAFETY_REPLACE_TEXT": ("text", WIDE, 2),
    "T_SAFETY_RESTORE_ASK": ("text", WIDE, 2), "T_SAFETY_RESTORE_TEXT": ("text", WIDE, 2),
    "T_SAFETY_RESTORED": ("text", 296, 2), "T_SAFETY_RESTORE_FAILED": ("text", 296, 2),
    "T_SAFETY_DELETE_ASK": ("text", WIDE, 2),
    # the backup by save: its row in the settings (on the left; its value on the right); what it does, when it is
    # turned on; a line of the sync's results; a save's versions (the option on its page, the list's title)
    "T_SET_SAVE_BACKUP": ("text", 300, 1), "T_SAVE_BACKUP_ON": ("text", 150, 1), "T_SAVE_BACKUP_OFF": ("text", 150, 1),
    "T_SAVE_BACKUP_NOTE": ("text", WIDE, 4), "T_SAVES_SENT_N": ("small", 400, 1), "T_SAVE_VERSIONS": ("text", 240, 1),
    "T_SAVE_NO_VERSIONS": ("text", 296, 3), "T_SAVE_VERSION_ASK": ("text", WIDE, 2), "T_SAVE_VERSION_TEXT": ("text", WIDE, 2),
    "T_SAVE_VERSION_DONE": ("text", 296, 1), "T_SAVE_VERSION_FAILED": ("text", 296, 2),
    # with no sd2psx, a Memory Card by its slot: a row of the Files group, and its name on the screens
    "T_MEMCARD_SLOT": ("text", 172, 1), "T_RENAME_VMC_TEXT": ("text", WIDE, 3), "T_MIGRATE_DELETE_ASK": ("text", WIDE, 2), "T_MIGRATE_DELETE_TEXT": ("text", WIDE, 3),
}
PARAGRAPH = ("text", DIALOG, 3)     # error messages and the rest: up to 3 lines in a box
# what only an sd2psx is ever told (being moved to another card, its boot cards, one more channel for a folder): not
# checked with another device's name
ONLY_SD2PSX = {"T_INSERT", "T_INSERT_NEEDS_BOOT", "T_INSERT_BOOT_ASK", "T_SWITCH_ASK", "T_SWITCH_FAILED",
               "T_SWITCH_BACK_FAILED", "T_INSTALL_RAISE_ASK", "T_INSTALL_BOOT_WARN", "T_SIZE_UNFORMATTED",
               "T_SIZE_NEXT_START", "T_SIZE_SET_FAILED"}

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
    (("T_BACK", "T_OPEN", "T_OPTIONS", "T_TPL_MAKE_MAIN"), False),
    (("T_CANCEL", "T_SKIP"), False), (("T_CANCEL_NO", "T_SKIP", "T_SYNC_DISABLE"), False), (("T_BACK", "T_MOVE"), False),
    # cards sent one after the other: the card on its way, and the list of them all (SELECT goes from one to the other)
    (("T_CANCEL", "T_SKIP", "T_VIEW"), False), (("T_CANCEL", "T_OPTIONS", "T_SKIP", "T_VIEW"), False),
    (("T_CANCEL", "T_UNDO", "T_VIEW"), False), (("T_CANCEL", "T_VIEW"), False),
    # saves marked for a copy: on a card's saves, and on the list of cards
    (("T_BACK", "T_MARK", "T_PASTE"), False), (("T_BACK", "T_OPEN", "T_PASTE"), False), (("T_BACK", "T_COPY"), False), (("T_BACK", "T_PASTE"), False),
    # a card's saves, with the options of how they are shown
    (("T_BACK", "T_OPEN", "T_OPTIONS", "T_INSTALL"), False), (("T_BACK", "T_TPL_CHANGE"), False),
    # a file of the Files group, with its options; renaming a folder of cards
    (("T_BACK", "T_OPEN", "T_INSTALL", "T_OPTIONS"), False), (("T_BACK", "T_OPEN", "T_INSTALL_SAVE", "T_OPTIONS"), False),
    (("T_BACK", "T_RUN", "T_OPTIONS"), False), (("T_BACK", "T_OPTIONS"), False), (("T_BACK", "T_RENAME"), False),
    # a folder picked for a new card file; "New card" on the main screen
    (("T_BACK", "T_OPEN", "T_CREATE_HERE"), False), (("T_BACK", "T_CREATE_HERE"), False),
    (("T_MENU_EXIT", "T_TPL_CREATE", "T_TOOLS", "T_SETTINGS"), 2), (("T_BACK", "T_REPLACE"), False),
    # a card's saves by game; the question of deleting them from it after
    (("T_BACK", "T_MARK", "T_MIGRATE_SIZE", "T_MIGRATE_START"), False), (("T_BACK", "T_MARK", "T_MIGRATE_START"), False),
    (("T_BACK", "T_MIGRATE_START"), False), (("T_MIGRATE_KEEP", "T_DELETE"), False),
]


LANG = ROOT / "lang"
REQUIRED = ("en", "pt")   # the languages that have every text
OWN = ("LANGUAGE_NAME", "DATE_FORMAT", "CONFIRM_BUTTON")   # what each language says of itself, beside its texts
DATE_FORMATS = ("DMY", "MDY", "YMD")   # or one of them with a separator of its own: D.M.Y, Y.M.D, D-M-Y...


def date_ok(f):
    order, seps = (f, "") if len(f) == 3 else (f[0::2], f[1::2])
    return len(f) in (3, 5) and order in DATE_FORMATS and len(set(seps)) <= 1 and all(c in "./-" for c in seps)
BUTTONS = {i for ids, _ in LEGENDS for i in ids}


def parse(text):
    """the (key, value) of a file of lang/, as lang.c reads it (the .properties format: comments start with # or !,
    the key ends at "=", ":" or a space, \\n \\t \\uXXXX and \\<any> in the value, a \\ at the end goes on in
    the next line)"""
    lines, out, i = text.lstrip("\ufeff").splitlines(), [], 0
    while i < len(lines):
        line = lines[i].lstrip(" \t\f")
        i += 1
        if not line or line[0] in "#!":
            continue
        m = re.match(r"([^=:\s]*)[ \t]*[=:]?[ \t]*", line)
        key, rest, value = m.group(1), line[m.end():], []
        while True:
            j, more = 0, False
            while j < len(rest):
                c, j = rest[j], j + 1
                if c != "\\":
                    value.append(c)
                elif j == len(rest):
                    more = True
                else:
                    c, j = rest[j], j + 1
                    if c == "u" and re.fullmatch(r"[0-9a-fA-F]{4}", rest[j:j + 4]):
                        value.append(chr(int(rest[j:j + 4], 16)))
                        j += 4
                    else:
                        value.append({"n": "\n", "t": "\t", "r": "\r"}.get(c, c))
            if more and i < len(lines):
                rest, i = lines[i].lstrip(" \t"), i + 1
                continue
            break
        out.append((key, "".join(value)))
    return out


def escape(s):
    """a text as the value of a line"""
    s = s.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t")
    return "\\" + s if s.startswith(" ") else s


# the texts that are more than words on the screen, and what that asks of them
LATIN_ONLY = {"T_TPL_DEFAULT": "Latin letters only: it becomes a folder's name, and the keyboard that edits it has no others."}


def kind(id_):
    """what the comment above a text says it is"""
    if id_ in BUTTONS:
        return "Button."
    lines = PLACE.get(id_, PARAGRAPH)[2]
    what = "One line." if lines == 1 else f"Up to {lines} lines."
    return f"{what} {LATIN_ONLY[id_]}" if id_ in LATIN_ONLY else what


def holes(s):
    """what the program fills in, in order"""
    return re.findall(r"%[0-9]*l*[sdu]", s)


def sync(ids, files):
    """each file of lang/ in the order of ids, with the comments above its texts"""
    english = dict(parse(files["en"].read_text(encoding="utf-8")))
    for code, path in files.items():
        lines = path.read_text(encoding="utf-8").splitlines()
        top = 0
        while top < len(lines) and (not lines[top].strip() or lines[top].lstrip()[0] in "#!"):
            top += 1
        texts = dict(parse(path.read_text(encoding="utf-8")))
        gone = [k for k in texts if k not in ids and k not in OWN]
        if gone:
            print(f"  {code}.txt: left out, as no text has these ids: {', '.join(gone)}")
        out = lines[:top] + [f"LANGUAGE_NAME = {texts.get('LANGUAGE_NAME', code)}",
                             "# how a date is written: DMY (31/12/2026), MDY (12/31/2026) or YMD (2026-12-31); with"
                             " another separator, D.M.Y (31.12.2026)",
                             f"DATE_FORMAT = {texts.get('DATE_FORMAT', 'YMD')}",
                             "# the button that confirms: CROSS, or CIRCLE as in Japan (the other one goes back)",
                             f"CONFIRM_BUTTON = {texts.get('CONFIRM_BUTTON', 'CROSS')}", ""]
        for id_ in ids:
            out.append(f"# {kind(id_)}" if code == "en" else f"# {kind(id_)} English: {escape(english.get(id_, ''))}")
            value = texts.get(id_, "")
            out.append(f"{id_} = {escape(value)}" if value else f"{id_} =")
        path.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")


def line_height(font):
    """a line of a paragraph, as ui.c spaces them: the font's height (FreeType's, rounded up) and 2 pixels"""
    hhea, upm = TTFont(TTF)["hhea"], TTFont(TTF)["head"].unitsPerEm
    px = SPEC[font][0]
    return -(-round((hhea.ascent - hhea.descent + hhea.lineGap) * px / upm * 64) // 64) + 2


def about_height(texts):
    """the box of Settings > About (settings.c, about_screen; dialog.c, scene_dialog): its title (the version), the
    texts with the gaps under each, the project's address and where the program is, and 28 pixels around them"""
    rows = [("text", "SD2Cloud v1.6.2 (98cce06) beta debug", 10), ("text", texts["T_ABOUT_TEXT"], 8),
            ("small", texts["T_ABOUT_CREDITS"], 4), ("small", texts["T_ABOUT_LICENSES"], 8),
            ("text", texts["T_ABOUT_AUTHOR"], 2), ("small", "github.com/oMrRexD/sd2cloud", 6),
            ("small", "mmce0:/APPS/SD2Cloud/SD2CLOUD.ELF", 0)]
    return 2 * 28 + sum(count_lines(font, s, 520 - 56) * line_height(font) + gap for font, s, gap in rows)


def legend_width(texts, apart, font="text"):
    """each button is a 20 px symbol, 6 px and its text; the last apart of them stand apart from the others, by a
    wider gap, and always 18 px from each other; five of them stand closer together (look_legend)"""
    gap, wide = (18, 34) if len(texts) > 4 else (26, 70)
    inner = apart - 1 if apart else 0
    w = sum(measure(font, t) + 26 for t in texts) + 18 * inner + gap * (len(texts) - 1 - inner)
    return w + (wide - gap if apart and len(texts) > apart else 0)


def main():
    ids = re.findall(r"^X\((\w+)\)", (ROOT / "src" / "ui" / "messages.def").read_text(encoding="utf-8"), re.M)
    files = {path.stem: path for path in sorted(LANG.glob("*.txt"))}
    if "--sync" in sys.argv:
        sync(ids, files)
    bad = 0
    langs = {}
    for code, path in files.items():
        pairs = parse(path.read_text(encoding="utf-8"))
        keys = [k for k, _ in pairs]
        for k in sorted({k for k in keys if keys.count(k) > 1}):
            bad += 1
            print(f"  TWICE        {k} [{code}]")
        for k in [k for k in keys if k not in ids and k not in OWN]:
            bad += 1
            print(f"  UNKNOWN ID   {k} [{code}] (not in messages.def)")
        texts = dict(pairs)
        if not re.fullmatch(r"[a-z]+", code):
            bad += 1
            print(f"  FILE NAME    {path.name}: a language's code is letters only (es.txt, fr.txt)")
        if not texts.get("LANGUAGE_NAME") or not date_ok(texts.get("DATE_FORMAT", "")) or \
                texts.get("CONFIRM_BUTTON", "CROSS").upper() not in ("CROSS", "CIRCLE"):
            bad += 1
            print(f"  [{code}] needs LANGUAGE_NAME, DATE_FORMAT = {', '.join(DATE_FORMATS)} (or D.M.Y...) and "
                  f"CONFIRM_BUTTON = CROSS or CIRCLE")
        for k, s in pairs:
            if k in LATIN_ONLY and not s.isascii():
                bad += 1
                print(f"  LATIN ONLY   {k} [{code}]: {LATIN_ONLY[k]}")
            lacking = sorted({c for c in s if ord(c) not in LETTERS and c not in "\n\t"})
            if lacking:
                bad += 1
                print(f"  NOT IN FONT  {k} [{code}]: {' '.join(f'{c} (U+{ord(c):04X})' for c in lacking)}")
        missing = [i for i in ids if not texts.get(i)]
        if missing and code in REQUIRED:
            bad += len(missing)
            print(f"  MISSING      [{code}] {', '.join(missing)}")
        elif missing:
            print(f"  [{code}] {len(missing)} text(s) not translated yet: shown in English")
        langs[code] = texts
    english = langs["en"]
    for code, texts in langs.items():
        for id_ in ids:
            if texts.get(id_) and holes(texts[id_]) != holes(english.get(id_, "")):
                bad += 1
                print(f"  %s AND %d    {id_} [{code}] has {holes(texts[id_])}, English {holes(english.get(id_, ''))}")
    for code, texts in langs.items():   # what the screen shows: a text not translated is the English one
        langs[code] = {i: texts.get(i) or english.get(i, "") for i in ids}
    if "--device" in sys.argv:
        name = sys.argv[sys.argv.index("--device") + 1]
        langs = {code: {i: s.replace("sd2psx", name) for i, s in texts.items() if i not in ONLY_SD2PSX}
                 for code, texts in langs.items()}
    for code, texts in langs.items():
        small = []
        for id_, s in texts.items():
            font, width, max_lines = PLACE.get(id_, PARAGRAPH)
            s = fill(s.replace("%s", texts[INSIDE[id_]], 1) if id_ in INSIDE else s, id_)
            w = measure(font, s)
            n = 1 if w <= width else count_lines(font, s, width)
            if n > max_lines:
                bad += 1
                print(f"  DOESN'T FIT  {id_} [{code}] {w} px / {width} px ({n} lines, max {max_lines}): {s}")
        for row, apart in LEGENDS:
            if all(i in texts for i in row):
                # (one longer than the line is written in the small letters: it has to fit in those)
                w = legend_width([texts[i] for i in row], apart)
                if w > 596 - 40 and legend_width([texts[i] for i in row], apart, "small") > 596 - 40:
                    bad += 1
                    print(f"  LEGEND TOO WIDE [{code}] {legend_width([texts[i] for i in row], apart, 'small')} px in "
                          f"the small letters: {' / '.join(texts[i] for i in row)}")
                elif w > 596 - 40:
                    small.append(" / ".join(texts[i] for i in row))
        # the five tabs in a row, 12 px apart at least (tabs.c): from x = 58, past TABS_W if they must, up to where
        # the right panel's title can start (x = 440, their arrow 18 px after them: 360 px)
        tabs = ("T_TAB_CARDS", "T_TAB_GAMES", "T_TAB_BOOT", "T_TAB_VMC", "T_TAB_FILES")
        w = sum(measure("small", texts[i]) for i in tabs) + 12 * (len(tabs) - 1)
        if w > 360:
            bad += 1
            print(f"  TABS TOO WIDE [{code}] {w} px / 360 px: {' / '.join(texts[i] for i in tabs)}")
        h = about_height(texts)
        if h > 372 - 74:   # (LOOK_BOTTOM - LOOK_TOP: between the header's line and the legend's)
            bad += 1
            print(f"  ABOUT TOO TALL [{code}] {h} px / {372 - 74} px: Settings > About")
        if small:
            print(f"  [{code}] {len(small)} legend(s) in the small letters, longer than the line: {'; '.join(small)}")
    print(f"{len(ids)} texts x {len(langs)} languages ({', '.join(langs)}) and {len(LEGENDS)} legends checked; "
          f"{bad} problem(s)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
