// Engine.cpp must never ask the app to delete more characters than precede the cursor in the text the app last
// reported: mutter turns delete_surrounding_text into byte offsets against that snapshot and aborts GNOME Shell
// (g_assert in meta_wayland_text_input_focus_delete_surrounding) when the request reaches before its start.
// Engine.cpp is included with IBus's delete call replaced by a mock that plays mutter.
// Build and run: bash Linux/Tests/delete_clamp_test.sh   (needs the IBus engine's build dependencies)
#include <ibus.h>
static int g_snapBefore = 0, g_violations = 0, g_deleted = 0;
static void mock_del(IBusEngine*, gint offset, guint nchars) { g_deleted += nchars; if (-offset > g_snapBefore) g_violations++; }
#define ibus_engine_delete_surrounding_text mock_del
#define ibus_engine_commit_text(e, t) ((void)0)
#define main engine_main
#include ENGINE_SRC
#undef main

static int fails = 0;
static void run(const char* name, const char* text, guint cursor, int n, int expectDeleted) {
    g_snapBefore = cursor; g_violations = g_deleted = 0;
    VikeyEngine* e = (VikeyEngine*)calloc(1, sizeof(VikeyEngine)); e->caps = IBUS_CAP_SURROUNDING_TEXT; _active = e;
    vikey_set_surrounding_text(IBUS_ENGINE(e), ibus_text_new_from_string(text), cursor, cursor); _surroundingSeen = true;
    outBackspaces(n);
    bool ok = g_violations == 0 && g_deleted == expectDeleted;
    printf("%s %-32s cursor=%u want=%d sent=%d%s\n", ok ? "ok  " : "FAIL", name, cursor, n, g_deleted, g_violations ? "  (mutter would abort)" : "");
    if (!ok) fails++;
    free(e);
}
int main() {
    run("normal delete",               "xin chao go", 11, 2, 2);
    run("cursor at the field start",   "go",           0, 2, 0);
    run("delete longer than the text", "ab",           2, 5, 2);
    run("empty text",                  "",             0, 1, 0);
    run("snapshot behind the cursor",  "hello world",  3, 6, 3);
    return fails ? 1 : 0;
}
