import SwiftUI
import UniformTypeIdentifiers

/// Gõ tắt (D68): the table of abbreviations. Add one with the two fields, remove with the minus; the table is saved
/// as it changes (macros.txt). Import takes a UniKey/OpenKey macro file, export writes one.
struct MacroEditor: View {
    @Environment(\.dismiss) private var dismiss
    @State private var rows: [(abbreviation: String, expansion: String)] = []
    @State private var abbreviation = ""
    @State private var expansion = ""
    @State private var message = ""
    @State private var query = ""

    private var shown: [(abbreviation: String, expansion: String)] {
        let q = query.trimmingCharacters(in: .whitespaces).lowercased()
        return q.isEmpty ? rows : rows.filter { $0.abbreviation.lowercased().contains(q) || $0.expansion.lowercased().contains(q) }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("Bảng gõ tắt").font(.headline)
                Spacer()
                TextField("Tìm", text: $query).textFieldStyle(.roundedBorder).frame(width: 130)
            }
            List {
                if shown.isEmpty {
                    Text(rows.isEmpty ? "Chưa có từ gõ tắt nào. Thêm ở dưới, ví dụ tks → cảm ơn." : "Không thấy.")
                        .foregroundStyle(.secondary)
                }
                ForEach(shown, id: \.abbreviation) { row in
                    HStack {
                        Text(row.abbreviation).fontWeight(.medium).frame(width: 90, alignment: .leading)
                        Text("→").foregroundStyle(.tertiary)
                        Text(row.expansion).lineLimit(2)
                        Spacer()
                        Button { VikeyCore.removeMacro(row.abbreviation); reload() } label: { Image(systemName: "minus.circle") }
                            .buttonStyle(.plain).foregroundStyle(.secondary)
                            .accessibilityLabel("Xoá \(row.abbreviation)")
                    }
                }
            }
            .frame(minHeight: 180)
            HStack {
                TextField("Gõ tắt", text: $abbreviation).textFieldStyle(.roundedBorder).frame(width: 100)
                    .onSubmit(add)
                Image(systemName: "arrow.right").foregroundStyle(.tertiary)
                TextField("Viết đầy đủ", text: $expansion).textFieldStyle(.roundedBorder)
                    .onSubmit(add)
                Button("Thêm", action: add).disabled(abbreviation.isEmpty || expansion.isEmpty)
            }
            Text(message.isEmpty ? "Gõ tắt xong bấm Space (hoặc Enter, Tab, dấu , . ;) để chuyển thành chữ đầy đủ." : message)
                .font(.callout).foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
            HStack {
                Button("Nhập từ tệp…", action: importFile)
                Button("Xuất ra tệp…", action: exportFile).disabled(rows.isEmpty)
                Spacer()
                Button("Xong") { dismiss() }.keyboardShortcut(.defaultAction)
            }
        }
        .padding(16)
        .frame(width: 440)
        .onAppear(perform: reload)
    }

    private func reload() {
        rows = VikeyCore.macros().map { ($0["abbreviation"] ?? "", $0["expansion"] ?? "") }
    }

    private func add() {
        let a = abbreviation.trimmingCharacters(in: .whitespacesAndNewlines)
        let e = expansion.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !a.isEmpty, !e.isEmpty else { return }
        guard VikeyCore.validMacroAbbreviation(a) else {
            message = "Gõ tắt chỉ gồm chữ và số, tối đa 32 ký tự, không có dấu cách."
            return
        }
        guard VikeyCore.setMacro(a, expansion: e) else { message = "Không thêm được."; return }
        message = ""; abbreviation = ""; expansion = ""
        reload()
    }

    private func importFile() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.plainText, .text]
        panel.allowsOtherFileTypes = true
        panel.message = "Chọn tệp gõ tắt của UniKey hoặc OpenKey (mỗi dòng: gõ tắt:đầy đủ)"
        guard panel.runModal() == .OK, let url = panel.url else { return }
        let n = VikeyCore.importMacros(fromPath: url.path)
        message = n < 0 ? "Không đọc được tệp." : "Đã nhập \(n) từ gõ tắt."
        reload()
    }

    private func exportFile() {
        let panel = NSSavePanel()
        panel.nameFieldStringValue = "vikey-go-tat.txt"
        panel.allowedContentTypes = [.plainText]
        guard panel.runModal() == .OK, let url = panel.url else { return }
        message = VikeyCore.exportMacros(toPath: url.path) ? "Đã xuất \(rows.count) từ gõ tắt." : "Không ghi được tệp."
    }
}
