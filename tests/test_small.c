/* SD2Cloud's regression tests -- the small parts: the tests' own SHA-256, the JSON reader, the .ini files (the
 * settings and the state). */
#include "t.h"

/* ------------------------------------------------------------ the SHA-256 the tests use, and files.c's hex of it */

static void sha256_examples(void)
{
    char hex[65];
    static unsigned char a[1000];
    sha256_hex((const unsigned char *)"", 0, hex);
    CHECK_STR(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    sha256_hex((const unsigned char *)"abc", 3, hex);
    CHECK_STR(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    /* over a block's end, and the padding going into one block more */
    sha256_hex((const unsigned char *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, hex);
    CHECK_STR(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    memset(a, 'a', sizeof(a));
    sha256_hex(a, sizeof(a), hex);
    CHECK_STR(hex, "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
}

void suite_host(void) { RUN(sha256_examples); }

/* ------------------------------------------------------------ json.c */

static int nFiles;
static char names[4][32];

static void on_file(const char *obj, size_t n, void *u)
{
    (void)u;
    if (nFiles < 4)
        js_get_string(obj, n, "name", names[nFiles], sizeof(names[0]));
    nFiles++;
}

static void json_values(void)
{
    static const char txt[] = "{ \"id\": \"abc\\\"123\", \"size\": \"8388608\", \"count\": 42, \"big\": 5000000000,"
                              " \"title\": \"caf\\u00e9\\n\", \"owner\": {\"name\": \"x\", \"id\": \"inner\"},"
                              " \"files\": [ {\"name\": \"one\"}, {\"name\": \"two\", \"more\": [1, 2, {\"name\": \"no\"}]} ],"
                              " \"empty\": [], \"last\": \"z\" }";
    const char *obj;
    size_t n = sizeof(txt) - 1, len;
    long long v;
    char s[64];
    CHECK_INT(js_get_string(txt, n, "id", s, sizeof(s)), 0);
    CHECK_STR(s, "abc\"123");
    CHECK_INT(js_get_number(txt, n, "size", &v), 0);   /* Google gives a file's size as a string */
    CHECK_INT(v, 8388608);
    CHECK_INT(js_get_number(txt, n, "count", &v), 0);
    CHECK_INT(v, 42);
    CHECK_INT(js_get_number(txt, n, "big", &v), 0);
    CHECK_INT(v, 5000000000LL);
    CHECK_INT(js_get_string(txt, n, "title", s, sizeof(s)), 0);
    CHECK_STR(s, "caf\xC3\xA9\n");   /* \uXXXX comes out as UTF-8 */
    CHECK_INT(js_get_string(txt, n, "last", s, sizeof(s)), 0);
    CHECK_STR(s, "z");
    CHECK(js_get_string(txt, n, "missing", s, sizeof(s)) != 0);
    /* a key is looked for at the top only: "name" is inside "owner" and inside "files" */
    CHECK(js_get_string(txt, n, "name", s, sizeof(s)) != 0);
    CHECK_INT(js_get_object(txt, n, "owner", &obj, &len), 0);
    CHECK_INT(js_get_string(obj, len, "id", s, sizeof(s)), 0);
    CHECK_STR(s, "inner");
    nFiles = 0;
    CHECK_INT(js_each(txt, n, "files", on_file, NULL), 0);
    CHECK_INT(nFiles, 2);
    CHECK_STR(names[0], "one");
    CHECK_STR(names[1], "two");
    nFiles = 0;
    CHECK_INT(js_each(txt, n, "empty", on_file, NULL), 0);
    CHECK_INT(nFiles, 0);
}

static void json_broken(void)
{
    static const char cut[] = "{ \"id\": \"abc", notJson[] = "<html>502</html>";
    char s[16];
    long long v;
    CHECK(js_get_string(cut, sizeof(cut) - 1, "id", s, sizeof(s)) != 0);
    CHECK(js_get_string(notJson, sizeof(notJson) - 1, "id", s, sizeof(s)) != 0);
    CHECK(js_get_number(notJson, sizeof(notJson) - 1, "id", &v) != 0);
    CHECK(js_each(notJson, sizeof(notJson) - 1, "files", on_file, NULL) != 0);
}

void suite_json(void)
{
    RUN(json_values);
    RUN(json_broken);
}

/* ------------------------------------------------------------ an .ini changed in place (files.c), and the settings */

static char seen[600];

static void on_key(const char *s, const char *k, const char *v, void *u)
{
    (void)u;
    snprintf(seen + strlen(seen), sizeof(seen) - strlen(seen), "[%s]%s=%s;", s, k, v);
}

static const char *ini_keys(const char *path)
{
    seen[0] = 0;
    ini_read(path, on_key, NULL);
    return seen;
}

static const char *text_of(const char *path)
{
    static char t[2000];
    buffer_t b = t_read(path);
    snprintf(t, sizeof(t), "%.*s", (int)b.len, b.data ? (const char *)b.data : "");
    buf_free(&b);
    return t;
}

static void ini_set_keeps_the_rest(void)
{
    static const char before[] = "; the user's own notes\r\n"
                                 "[general]\r\n"
                                 "language = pt   ; mine\r\n"
                                 "\r\n"
                                 "[igr]\r\n"
                                 "; return = somewhere\r\n"
                                 "return = auto\r\n"
                                 "return_name = x\r\n"
                                 "\r\n"
                                 "[retention]\r\n"
                                 "default = 10\r\n";
    t_text("a.ini", before);
    /* a key that is there: its line alone changes (not the comment that only looks like it, nor the key that
     * starts the same way) */
    CHECK_INT(ini_set("a.ini", "igr", "return", "mmce?:/APPS/x.elf"), 0);
    CHECK_STR(text_of("a.ini"), "; the user's own notes\r\n"
                                "[general]\r\n"
                                "language = pt   ; mine\r\n"
                                "\r\n"
                                "[igr]\r\n"
                                "; return = somewhere\r\n"
                                "return = mmce?:/APPS/x.elf\r\n"
                                "return_name = x\r\n"
                                "\r\n"
                                "[retention]\r\n"
                                "default = 10\r\n");
    /* a key that isn't: after the last line of its section, before the blank line */
    CHECK_INT(ini_set("a.ini", "general", "ask_connect", "no"), 0);
    /* (read back: a comment after a value isn't part of it) */
    CHECK_STR(ini_keys("a.ini"), "[general]language=pt;[general]ask_connect=no;[igr]return=mmce?:/APPS/x.elf;"
                                 "[igr]return_name=x;[retention]default=10;");
    CHECK(strstr(text_of("a.ini"), "language = pt   ; mine\r\nask_connect = no\r\n\r\n[igr]") != NULL);
    /* a section that isn't: at the end */
    CHECK_INT(ini_set("a.ini", "app", "app_path", "mmce?:/A.ELF"), 0);
    CHECK(strstr(text_of("a.ini"), "default = 10\r\n\r\n[app]\r\napp_path = mmce?:/A.ELF\r\n") != NULL);
    CHECK(strstr(text_of("a.ini"), "; the user's own notes\r\n[general]") != NULL);
    /* a file that isn't there is made */
    CHECK_INT(ini_set("new.ini", "s", "k", "v"), 0);
    CHECK_STR(text_of("new.ini"), "[s]\r\nk = v\r\n");
}

static void settings_read_and_set(void)
{
    /* no file: the defaults, and setting something makes the file, from the one the program embeds */
    config_read();
    CHECK_INT(configExists, 0);
    CHECK_STR(cfg.drive_folder, "PS2 Memory Card Backups");
    CHECK_INT(cfg.keep, 10);
    CHECK_INT(cfg.types, TYPE_NORMAL | TYPE_GAMEID | TYPE_NAMED);
    CHECK_INT(config_set("general", "language", "pt"), 0);
    CHECK(t_size("sd/SD2Cloud/sd2cloud.ini") > 0);
    t_text("sd/SD2Cloud/sd2cloud.ini", "[general]\nlanguage = en\ndrive_folder = My Cards\n"
                                       "[cards]\ntypes = normal, boot\nexclude = Card2/Card2-1\n"
                                       "[retention]\ndefault = 0\nCard1/Card1-1 = 20\nCard3 = 5\n"
                                       "[igr]\nauto_sync = no\n");
    config_read();
    CHECK_INT(configExists, 1);
    CHECK_STR(cfg.language, "en");
    CHECK_STR(cfg.drive_folder, "My Cards");
    CHECK_INT(cfg.types, TYPE_NORMAL | TYPE_BOOT);
    CHECK_INT(cfg.keep, 0);
    CHECK_INT(cfg.no_auto_sync, 1);
    CHECK_INT(config_keep("Card1/Card1-1"), 20);
    CHECK_INT(config_keep("Card3/Card3-2"), 5);     /* a rule for the folder covers its cards */
    CHECK_INT(config_keep("Card9/Card9-1"), 0);
    CHECK(list_has(cfg.exclude, "Card2/Card2-1"));
    CHECK(!list_has(cfg.exclude, "Card2/Card2-2"));
}

/* ------------------------------------------------------------ state.c */

static void state_comes_back(void)
{
    card_state_t *e;
    state_read();
    CHECK(state_empty());
    e = state_card("Card1/Card1-1", 1);
    CHECK(e != NULL);
    snprintf(e->fingerprint, sizeof(e->fingerprint), "%s", "f1");
    e->version = MCFS_VERSION;
    CHECK(state_empty());   /* seen, but never backed up */
    snprintf(e->sha, sizeof(e->sha), "%s", "s1");
    snprintf(e->when, sizeof(e->when), "%s", "2026-01-02 03:04:05");
    state_set_folder("PS2 Memory Card Backups/Card1", "driveId1");
    CHECK_INT(state_write(), 0);
    state_card("Card9/Card9-9", 1);   /* (never written: gone when the file is read again) */
    state_read();
    CHECK(!state_empty());
    CHECK(state_card("Card9/Card9-9", 0) == NULL);
    e = state_card("card1/CARD1-1", 0);   /* a card's name is the same in any case */
    CHECK(e != NULL);
    if (e) {
        CHECK_STR(e->fingerprint, "f1");
        CHECK_STR(e->sha, "s1");
        CHECK_STR(e->when, "2026-01-02 03:04:05");
        CHECK_INT(e->version, MCFS_VERSION);
    }
    CHECK_STR(state_folder("PS2 Memory Card Backups/Card1"), "driveId1");
    CHECK(state_folder("PS2 Memory Card Backups/Card2") == NULL);
}

void suite_ini(void)
{
    RUN(ini_set_keeps_the_rest);
    RUN(settings_read_and_set);
}

void suite_state(void) { RUN(state_comes_back); }
