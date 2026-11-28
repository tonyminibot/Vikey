// vikey-settings --macros: the gõ tắt table (D68). The table is ~/.config/vikey/macros.txt (Engine/Macro.cpp); every
// change is saved at once and MacroRevision in settings.json is bumped so the IBus engine reloads the file.
#include "Common.h"
#include "Macro.h"
#include <glib/gstdio.h>
#include <fstream>
#include <sstream>
using namespace vikey;

static GtkWidget *_mWin, *_mList, *_mAbbr, *_mExp, *_mMsg;
static const char* kHint = "Gõ tắt xong bấm Space (hoặc Enter, Tab, dấu , . ;) để chuyển thành chữ đầy đủ.";
static std::string macroFile() { return prefs::configDir() + "/macros.txt"; }
static void saveTable() {
    g_mkdir_with_parents(prefs::configDir().c_str(), 0700);
    macro::save(macroFile());
    prefs::setInt("MacroRevision", prefs::getInt("MacroRevision", 0) + 1);   // settings.json changed: the engine reloads both files
}
static void say(const std::string& s) { gtk_label_set_text(GTK_LABEL(_mMsg), s.empty() ? kHint : s.c_str()); }
static void refill();
static void onDelete(GtkButton* b, gpointer) {
    const char* a = (const char*)g_object_get_data(G_OBJECT(b), "abbr");
    if (a && macro::remove(a)) { saveTable(); refill(); }
}
static void refill() {
    while (GtkWidget* c = gtk_widget_get_first_child(_mList)) gtk_list_box_remove(GTK_LIST_BOX(_mList), c);
    auto rows = macro::all();
    if (rows.empty()) {
        GtkWidget* r = adw_action_row_new(); adw_preferences_row_set_title(ADW_PREFERENCES_ROW(r), "Chưa có từ gõ tắt nào");
        adw_action_row_set_subtitle(ADW_ACTION_ROW(r), "Thêm ở dưới, ví dụ tks → cảm ơn."); gtk_list_box_append(GTK_LIST_BOX(_mList), r);
        return;
    }
    for (auto& kv : rows) {
        gchar* t = g_markup_escape_text(kv.first.c_str(), -1); gchar* s = g_markup_escape_text(kv.second.c_str(), -1);
        GtkWidget* r = adw_action_row_new(); adw_preferences_row_set_title(ADW_PREFERENCES_ROW(r), t); adw_action_row_set_subtitle(ADW_ACTION_ROW(r), s);
        g_free(t); g_free(s);
        GtkWidget* del = gtk_button_new_from_icon_name("user-trash-symbolic"); gtk_widget_add_css_class(del, "flat"); gtk_widget_set_valign(del, GTK_ALIGN_CENTER);
        gtk_widget_set_tooltip_text(del, "Xoá");
        g_object_set_data_full(G_OBJECT(del), "abbr", g_strdup(kv.first.c_str()), g_free);
        g_signal_connect(del, "clicked", G_CALLBACK(onDelete), nullptr);
        adw_action_row_add_suffix(ADW_ACTION_ROW(r), del);
        gtk_list_box_append(GTK_LIST_BOX(_mList), r);
    }
}
static std::string trimmed(const char* s) { std::string t = s ? s : ""; size_t a = t.find_first_not_of(" \t\r\n"); if (a == std::string::npos) return ""; return t.substr(a, t.find_last_not_of(" \t\r\n") - a + 1); }
static void onAdd(GtkWidget*, gpointer) {
    std::string a = trimmed(gtk_editable_get_text(GTK_EDITABLE(_mAbbr))), e = trimmed(gtk_editable_get_text(GTK_EDITABLE(_mExp)));
    if (a.empty() || e.empty()) return;
    if (!macro::validAbbreviation(a)) { say("Gõ tắt chỉ gồm chữ và số, tối đa 32 ký tự, không có dấu cách."); return; }
    if (!macro::set(a, e)) { say("Không thêm được."); return; }
    saveTable(); say(""); gtk_editable_set_text(GTK_EDITABLE(_mAbbr), ""); gtk_editable_set_text(GTK_EDITABLE(_mExp), ""); refill(); gtk_widget_grab_focus(_mAbbr);
}
static void onImported(GObject* src, GAsyncResult* res, gpointer) {
    GFile* f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, nullptr); if (!f) return;
    char* data = nullptr; gsize len = 0;
    if (!g_file_load_contents(f, nullptr, &data, &len, nullptr, nullptr)) { say("Không đọc được tệp."); g_object_unref(f); return; }
    auto before = macro::all();
    macro::parse(std::string(data, len)); size_t n = macro::count();
    for (auto& kv : before) if (!macro::has(kv.first)) macro::set(kv.first, kv.second);   // added to the table, the file's entry wins
    g_free(data); g_object_unref(f); saveTable(); refill(); say("Đã nhập " + std::to_string(n) + " từ gõ tắt.");
}
static void onExported(GObject* src, GAsyncResult* res, gpointer) {
    GFile* f = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(src), res, nullptr); if (!f) return;
    char* p = g_file_get_path(f); bool ok = p && macro::save(p); g_free(p); g_object_unref(f);
    say(ok ? "Đã xuất " + std::to_string(macro::count()) + " từ gõ tắt." : "Không ghi được tệp.");
}
static void onImport(GtkButton*, gpointer) { GtkFileDialog* d = gtk_file_dialog_new(); gtk_file_dialog_set_title(d, "Chọn tệp gõ tắt của UniKey hoặc OpenKey"); gtk_file_dialog_open(d, GTK_WINDOW(_mWin), nullptr, onImported, nullptr); g_object_unref(d); }
static void onExport(GtkButton*, gpointer) { GtkFileDialog* d = gtk_file_dialog_new(); gtk_file_dialog_set_initial_name(d, "vikey-go-tat.txt"); gtk_file_dialog_save(d, GTK_WINDOW(_mWin), nullptr, onExported, nullptr); g_object_unref(d); }

void buildMacros(GtkApplication* app) {
    macro::load(macroFile());
    _mWin = adw_application_window_new(app); gtk_window_set_title(GTK_WINDOW(_mWin), "Bảng gõ tắt"); gtk_window_set_default_size(GTK_WINDOW(_mWin), 460, 560);
    GtkWidget* tv = adw_toolbar_view_new(); GtkWidget* hb = adw_header_bar_new(); adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tv), hb);
    GtkWidget* imp = gtk_button_new_with_label("Nhập…"); g_signal_connect(imp, "clicked", G_CALLBACK(onImport), nullptr); adw_header_bar_pack_start(ADW_HEADER_BAR(hb), imp);
    GtkWidget* exp = gtk_button_new_with_label("Xuất…"); g_signal_connect(exp, "clicked", G_CALLBACK(onExport), nullptr); adw_header_bar_pack_start(ADW_HEADER_BAR(hb), exp);
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 16); gtk_widget_set_margin_end(box, 16); gtk_widget_set_margin_top(box, 12); gtk_widget_set_margin_bottom(box, 16);
    // add: abbreviation, expansion, button
    GtkWidget* add = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    _mAbbr = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(_mAbbr), "Gõ tắt"); gtk_editable_set_width_chars(GTK_EDITABLE(_mAbbr), 9);
    _mExp = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(_mExp), "Viết đầy đủ"); gtk_widget_set_hexpand(_mExp, TRUE);
    GtkWidget* btn = gtk_button_new_with_label("Thêm"); gtk_widget_add_css_class(btn, "suggested-action");
    g_signal_connect(_mAbbr, "activate", G_CALLBACK(onAdd), nullptr); g_signal_connect(_mExp, "activate", G_CALLBACK(onAdd), nullptr);
    g_signal_connect(btn, "clicked", G_CALLBACK(onAdd), nullptr);
    gtk_box_append(GTK_BOX(add), _mAbbr); gtk_box_append(GTK_BOX(add), _mExp); gtk_box_append(GTK_BOX(add), btn); gtk_box_append(GTK_BOX(box), add);
    _mMsg = gtk_label_new(kHint); gtk_widget_add_css_class(_mMsg, "dim-label"); gtk_label_set_wrap(GTK_LABEL(_mMsg), TRUE); gtk_label_set_xalign(GTK_LABEL(_mMsg), 0); gtk_box_append(GTK_BOX(box), _mMsg);
    // the table
    _mList = gtk_list_box_new(); gtk_list_box_set_selection_mode(GTK_LIST_BOX(_mList), GTK_SELECTION_NONE); gtk_widget_add_css_class(_mList, "boxed-list");
    GtkWidget* sw = gtk_scrolled_window_new(); gtk_widget_set_vexpand(sw, TRUE); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), _mList);
    gtk_box_append(GTK_BOX(box), sw);
    refill();
    g_signal_connect(_mWin, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer) { _mWin = nullptr; }), nullptr);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tv), box); adw_application_window_set_content(ADW_APPLICATION_WINDOW(_mWin), tv);
    gtk_window_present(GTK_WINDOW(_mWin));
}
