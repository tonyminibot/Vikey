# PLAN: Add Cross-Platform Text Expansion ("Gõ tắt") to Vikey

> Target repository: `harrytranft/Vikey`  
> Working branch: `feature/text-expansion`  
> Baseline: forked from `tonyminibot/Vikey`, Release 1.0.33  
> Goal: add reliable text expansion / macro typing for Windows and macOS without regressing Vikey's existing Vietnamese engine, auto English/Vietnamese switching, undo, navigation, secure-input handling, or app compatibility.

---

## 1. Objective

Add a first-class **Gõ tắt / Text Expansion** feature to Vikey, similar in user-facing behavior to OpenKey/UniKey macros, while fitting Vikey's current architecture.

Examples:

```
ss + Space  -> Sun Santa 
dc + Space  -> được 
ph + ,      -> phụ huynh,
mail + Space -> example@gmail.com 
```

The implementation must work on:

- Windows
- macOS

The feature must support:

- enable/disable
- use in Vietnamese mode
- optional use in English mode
- trigger/replacement table
- add/edit/delete
- search/filter
- import OpenKey/UniKey-compatible macro file
- export OpenKey/UniKey-compatible macro file
- Unicode replacement text
- emoji
- multiline replacement
- punctuation triggers
- prevention of recursive expansion
- secure/password-field protection
- optional automatic capitalization
- migration from an existing OpenKey installation on Windows where practical

---

## 2. Important architectural decision

### Do NOT copy OpenKey's old macro engine directly into Vikey's current `Engine.cpp`.

OpenKey historically implemented macros inside the engine with:

- `Macro.h`
- `Macro.cpp`
- `vReplaceMaro`
- `vKeyHookState::macroKey`
- `vKeyHookState::macroData`

Current Vikey intentionally no longer carries those macro vectors in `vKeyHookState`. It keeps only the enum slot:

```cpp
vReplaceMacro_unused
```

This is important because current Vikey's engine now contains considerably more logic than old OpenKey:

- automatic English/Vietnamese switching
- English-word buffering
- look-back correction
- doubled-tone-key handling
- auto-fix undo
- navigation recovery
- teencode rules
- Simple Telex variants
- spelling restore behavior

Reintroducing the old OpenKey macro flow directly into `Engine.cpp` risks coupling text expansion to all of those state machines.

### Preferred architecture

Implement text expansion as a **shared cross-platform module outside the Vietnamese transformation engine**, then integrate it in the platform cores:

- Windows: `Windows/App/Core.cpp`
- macOS: `App/Core/VikeyCore.mm`

The text-expansion layer should track the text currently visible for the current token and act at word-boundary keys before normal auto-switch processing.

High-level pipeline:

```
physical key
    |
    v
platform keyboard hook / event tap
    |
    +--> update TextExpansionSession
    |
    +--> if boundary key:
    |       |
    |       +--> expansion matched?
    |               |
    |               +--> delete trigger text
    |               +--> insert replacement
    |               +--> insert original boundary key
    |               +--> reset Vikey typing context
    |               +--> consume physical event
    |
    +--> otherwise continue existing Vikey processing
            |
            +--> Vietnamese engine
            +--> auto switch
            +--> restore / undo logic
```

This keeps the most dangerous engine code minimally changed.

---

## 3. Existing Vikey behavior that MUST be preserved

Before implementation, inspect the current versions of:

```
Engine/Engine.cpp
Engine/Engine.h
Engine/DataType.h
Engine/Driver.cpp
Engine/Driver.h

App/Core/VikeyCore.mm
App/Core/VikeyCore.h
App/Model/Preferences.swift
App/Views/MainView.swift

Windows/App/Core.cpp
Windows/App/Prefs.cpp
Windows/App/Prefs.h
Windows/App/Settings.cpp
Windows/App/CMakeLists.txt

project.yml
```

Important existing mechanisms:

### 3.1 Synthetic event recursion protection

macOS already tags Vikey-generated events with:

```cpp
VIKEY_EVENT_TAG
kCGEventSourceUserData
```

Windows already tags Vikey-generated events with:

```cpp
VIKEY_EVENT_TAG
dwExtraInfo
```

Reuse this. Do not create a second event-hook system.

### 3.2 Current output path

macOS already has helpers around:

- `SendBackspace()`
- `PostText(...)`
- `SendEngineOutput()`
- `+insertText:`

Windows already has helpers around:

- `batchBackspaces(...)`
- `batchText(...)`
- `batchKey(...)`
- `flushBatch()`
- `SendEngineOutput()`
- `SendInput(...)`

Text expansion should reuse these existing output paths.

### 3.3 Secure/password protections

Windows currently has:

```cpp
site::focusedIsPassword()
```

and typing-log logic already excludes password managers / terminals.

macOS already has secure-input awareness for typing logs and platform event handling.

Text expansion MUST NOT expand inside password/secure-input fields when the platform can detect that state.

---

## 4. Shared module design

Create:

```
Engine/TextExpansion.h
Engine/TextExpansion.cpp
```

Do not use the name `Shortcut`, because Vikey already uses "shortcut" terminology for keyboard shortcuts and internal states such as `_enAfterShortcut`.

### 4.1 Data model

Suggested public model:

```cpp
struct TextExpansionEntry {
    std::u16string trigger;
    std::u16string replacement;
};

struct TextExpansionOptions {
    bool enabled = true;
    bool inEnglishMode = true;
    bool autoCaps = false;
};

struct TextExpansionMatch {
    bool matched = false;
    int deleteCount = 0;
    std::u16string replacement;
};
```

### 4.2 Manager

Suggested API:

```cpp
class TextExpansionManager {
public:
    void setEntries(const std::vector<TextExpansionEntry>& entries);
    const std::vector<TextExpansionEntry>& entries() const;

    bool addOrUpdate(const TextExpansionEntry& entry);
    bool remove(const std::u16string& trigger);
    void clear();

    TextExpansionMatch find(
        const std::u16string& visibleToken,
        bool autoCaps
    ) const;
};
```

Use a hash map internally for exact matching.

Avoid converting through legacy Vikey keyboard-code data unless necessary. The text-expansion model should use Unicode text.

### 4.3 Per-input session

Create a small session/state object:

```cpp
class TextExpansionSession {
public:
    void reset();

    void append(char16_t ch);
    void erase(int count);

    // Apply an edit that Vikey itself made to the currently visible token.
    void applyEdit(int deleteCount, const std::u16string& insertedText);

    const std::u16string& token() const;

private:
    std::u16string token_;
};
```

The session must represent the **currently visible token**, not merely raw physical keys.

Example:

User types raw Telex:

```
d d u o c
```

Visible text may become:

```
được
```

If Vikey's engine changes previously typed characters, call `applyEdit()` with the same edit so the expansion session remains synchronized with what the user sees.

---

## 5. Trigger behavior

Expansion is checked only on configured boundary keys.

For v1, support:

- Space
- Enter
- Tab
- comma
- period
- semicolon
- colon
- exclamation mark
- question mark
- closing parenthesis
- closing square bracket
- closing brace

Examples:

```
dc[Space] -> được[Space]
dc,       -> được,
dc.       -> được.
dc?       -> được?
```

Do NOT expand in the middle of a token.

If trigger is:

```
dc
```

then:

```
abcdc
```

must NOT expand.

### 5.1 Token reset

Reset the text-expansion token when context becomes unreliable, including:

- mouse click
- app focus change
- cursor navigation
- Home/End
- Ctrl/Cmd shortcuts that can alter text
- paste
- cut
- undo/redo
- language/manual switch where current word context is discarded
- secure-input transition
- any platform condition where Vikey already resets typing context

Reuse current Vikey reset points whenever possible.

---

## 6. Order relative to Auto Switch

This is critical.

When the user presses a boundary key:

1. inspect the text-expansion session
2. if a matching trigger exists and expansion is allowed:
   - perform expansion
   - preserve the original boundary key
   - reset relevant Vikey word/auto-switch context
   - consume the original event
3. if no expansion exists:
   - continue the existing Vikey auto-switch/engine path unchanged

Do not allow Auto Switch to analyze and correct the trigger before expansion.

Example:

```
dc + Space
```

should become:

```
được 
```

It must not first be interpreted as a normal English/Vietnamese word by auto-switch.

---

## 7. Applying an expansion

For a match:

```
visible token: dc
replacement:   được
boundary:      Space
```

Expected operation:

```
delete 2 visible UTF-16 code units/characters as appropriate
insert "được"
insert original Space
reset current Vikey word context
```

### 7.1 Do not use `vKeyHookState::charData` for long replacement text

Current engine buffer:

```cpp
#define MAX_BUFF 32
Uint32 charData[MAX_BUFF];
```

is not suitable for real-world text expansion.

Users must be able to store longer content such as:

```
Kính gửi Quý Phụ huynh,
Sun Santa xin thông báo...
```

Therefore platform insertion should use the platform's existing Unicode text-output helper, not the engine's fixed 32-character buffer.

### 7.2 Multiline

A replacement may contain:

- `\n`
- emoji
- Vietnamese Unicode
- punctuation
- long text

Do not artificially limit replacement length to `MAX_BUFF`.

Set a reasonable storage/UI safety limit if desired, e.g. 16 KB per entry, but keep the input engine independent of that limit.

---

## 8. Auto Caps behavior

Optional setting:

```
Tự viết hoa nội dung gõ tắt
```

Compatibility target should be close to OpenKey behavior.

Recommended v1 rules:

Given:

```
dc -> được
```

Then:

```
dc  -> được
Dc  -> Được
DC  -> ĐƯỢC
```

Only apply this behavior when Auto Caps is enabled.

For Unicode Vietnamese text, use Unicode-aware case conversion at the platform/storage layer or a shared helper that correctly handles Vietnamese characters.

Do not implement ASCII-only `toupper()` behavior for user-facing Unicode strings.

Document exact rules in tests.

---

## 9. Settings / storage

Use the following setting names for compatibility with remnants already present in the macOS project:

```
UseMacro
UseMacroInEnglishMode
vAutoCapsMacro
```

These keys already exist in `App/Model/Preferences.swift`.

For clarity in new code, the internal feature can be called Text Expansion while preserving these preference keys for backward compatibility.

### 9.1 macOS

Use `UserDefaults` for small scalar settings.

For the entry list, prefer a dedicated JSON file under Vikey Application Support rather than embedding an unbounded macro list into UserDefaults.

Suggested path:

```
~/Library/Application Support/Vikey/text-expansions.json
```

Example:

```json
{
  "version": 1,
  "entries": [
    {
      "trigger": "dc",
      "replacement": "được"
    },
    {
      "trigger": "ss",
      "replacement": "Sun Santa"
    }
  ]
}
```

Write atomically.

### 9.2 Windows

Use existing registry preferences only for scalar toggles:

```
HKCU\Software\Vikey
```

Store entries in:

```
%LOCALAPPDATA%\Vikey\text-expansions.json
```

Reuse `prefs::appDataDir()`.

This avoids oversized Registry values and gives both platforms conceptually identical JSON storage.

### 9.3 Shared file format

Use UTF-8 JSON, schema version 1.

Required fields:

```
version
entries[].trigger
entries[].replacement
```

Reject or skip malformed entries safely.

Trim neither trigger nor replacement automatically unless UI explicitly tells the user.

A replacement may intentionally begin/end with whitespace.

---

## 10. OpenKey / UniKey-compatible import/export

OpenKey supports a text format similar to:

```
;Compatible OpenKey Macro Data file for UniKey*** version=1 ***
dc:được
ph:phụ huynh
ss:Sun Santa
```

Implement import/export for this format.

### Import requirements

- UTF-8
- ignore first metadata/header line when present
- skip blank lines
- split on the first valid delimiter according to OpenKey-compatible behavior
- preserve colons inside replacement text
- handle duplicate triggers:
  - UI should allow "Replace existing" or use deterministic overwrite behavior
- report:
  - imported count
  - replaced count
  - skipped/invalid count

### Export requirements

Header:

```
;Compatible OpenKey Macro Data file for UniKey*** version=1 ***
```

Then one entry per line.

Round trip must preserve normal Vietnamese text.

---

## 11. Migration from OpenKey on Windows

Current Vikey already has:

```cpp
prefs::migrateFromOpenKey()
```

in:

```
Windows/App/Prefs.cpp
```

It currently migrates several OpenKey preferences.

Extend migration carefully.

OpenKey historically stored binary macro data under:

```
HKCU\SOFTWARE\TuyenMai\OpenKey\macroData
```

### Preferred migration strategy

Do NOT copy the old binary blob directly into new storage.

Instead:

1. read `macroData`
2. parse the legacy OpenKey binary format in a small compatibility decoder
3. convert each entry to `TextExpansionEntry`
4. save to the new JSON storage

Legacy OpenKey structure:

- first 2 bytes: macro count
- for each macro:
  - 1 byte trigger byte length
  - trigger UTF-8 bytes
  - 2 bytes replacement byte length
  - replacement UTF-8 bytes

Add robust length/bounds checks. Never trust registry data.

Migration must be one-time and idempotent.

Suggested flag:

```
MigratedTextExpansionsFromOpenKey
```

If Vikey already has text expansions, do not silently overwrite them. Merge non-conflicting entries.

---

## 12. macOS UI

Current main tabs in:

```
App/Views/MainView.swift
```

include:

- Chung
- Tự chuyển
- Gõ tiếng Việt
- Emoji
- Loại trừ

Add:

```
Gõ tắt
```

Suggested order:

```
Chung
Tự chuyển
Gõ tiếng Việt
Gõ tắt
Emoji
Loại trừ
```

If `MainView.swift` becomes too large, create:

```
App/Views/TextExpansionView.swift
App/Model/TextExpansionStore.swift
```

Recommended UI:

```
[✓] Bật gõ tắt
[✓] Gõ tắt cả khi đang ở chế độ tiếng Anh
[ ] Tự viết hoa

Search...

Trigger              Replacement
----------------------------------------------
dc                   được
ph                   phụ huynh
ss                   Sun Santa
...

+ Thêm      Sửa      Xoá

Nhập...                         Xuất...
```

Requirements:

- trigger field required
- replacement required
- prevent empty trigger
- show duplicate trigger error or edit existing
- keyboard-accessible
- sensible table/list scrolling
- large replacement editor supports multiline
- user can copy/paste emoji
- settings apply without restarting Vikey

When entries change, core must reload safely.

---

## 13. Windows UI

Use current settings architecture in:

```
Windows/App/Settings.cpp
Windows/App/Settings.h
```

Add a dedicated Text Expansion section/page matching current Vikey visual style.

Feature parity with macOS:

- enable
- allow in English
- auto caps
- list
- search
- add
- edit
- delete
- import
- export

Do not block the keyboard hook while expensive file dialogs or large UI operations occur.

When settings are saved:

- update persistent storage
- reload text expansion manager
- avoid full app restart

---

## 14. Core integration: Windows

Primary file:

```
Windows/App/Core.cpp
```

### 14.1 State

Add a manager and per-context session.

Do not duplicate event injection.

The current code already maintains English-mode state including:

```
_enKeys
_enCount
_enValid
_enAfterShortcut
...
```

Text expansion must be coordinated with this state but should not be embedded into it.

### 14.2 Keyboard-hook order

Before the normal English/Vietnamese word-end flow processes a supported boundary key:

- check whether feature is enabled
- check English-mode permission if `vLanguage == 0`
- check secure/password state
- check current token
- match expansion

If matched:

1. queue backspaces for the visible trigger length
2. queue replacement text
3. queue original boundary key
4. flush batch
5. reset:
   - text expansion session
   - English word state
   - engine current session
   - auto-switch lookback as appropriate
   - auto-fix undo state
6. return consumed

Preserve Shift+Enter / special boundary semantics where relevant.

### 14.3 Injected events

All inserted text / keys must carry `VIKEY_EVENT_TAG` via existing helpers.

Do not allow the inserted replacement itself to trigger another expansion.

Example:

```
a -> b
b -> c
```

typing `a + Space` must expand once to `b `, not recursively to `c `.

---

## 15. Core integration: macOS

Primary file:

```
App/Core/VikeyCore.mm
```

Apply the same behavior as Windows.

Reuse:

- current event tap
- `VIKEY_EVENT_TAG`
- `PostText`
- `SendBackspace`
- existing keyboard/key-code re-posting helpers
- `+insertText:` where useful

Do not create a new global event tap.

After a successful expansion, reset the same logical contexts as Windows.

macOS and Windows behavior should be covered by shared logic wherever practical.

---

## 16. Keep TextExpansionSession synchronized with visible text

This is the hardest correctness issue.

When a normal physical letter is passed through unchanged:

```
session.append(character)
```

When Vikey modifies existing text using an engine edit:

```
deleteCount = pData->backspaceCount
insertedText = characters represented by pData->charData
session.applyEdit(deleteCount, insertedText)
```

If the engine also re-sends a triggering key as part of `vRestore`, account for it exactly once.

Do not assume raw key count == visible text length.

Particular cases to test:

- `dd` -> `đ`
- `aw` -> `ă`
- `ow` -> `ơ`
- tone placement changes
- backspace after tone transformation
- restored invalid Vietnamese spelling
- auto switch restoring typed English keys

If synchronization becomes unreliable in any branch, reset the expansion session instead of making a risky match.

False negatives are preferable to expanding the wrong text.

---

## 17. Secure input and excluded contexts

### Windows

Do not expand when:

```cpp
site::focusedIsPassword()
```

returns true.

Consider excluding known password managers and terminal apps using the same safety rationale as typing-log key capture.

### macOS

Do not expand during macOS secure input.

If secure-input detection is not currently exposed as one reusable function, refactor existing secure-input checks into a helper shared by typing log and text expansion.

### General

Do not persist or log the actual user's typed trigger history.

The expansion feature needs only the small current token in memory.

---

## 18. Interaction with Backspace

Before expansion:

```
d c Backspace
```

must leave session = `d`.

After expansion:

```
dc Space
```

produces:

```
được 
```

Then:

```
Backspace
```

should behave like a normal backspace on the inserted boundary.

Do NOT hook text expansion into Vikey's existing AutoFixUndo behavior unless there is a clearly defined feature requirement.

For v1, expansion itself does not require a special "undo macro" mode.

Ctrl+Z / Cmd+Z should remain the application's own undo behavior.

---

## 19. Interaction with navigation

Current Vikey has complex left/right navigation handling for auto switch.

Text expansion should be conservative.

On:

- Left
- Right
- Up
- Down
- Home
- End
- mouse click

reset the expansion session unless the current architecture can prove cursor/token continuity.

Do not try to reconstruct arbitrary application text around the cursor.

---

## 20. Interaction with clipboard shortcuts

On:

- Ctrl/Cmd+V
- Ctrl/Cmd+X
- Ctrl/Cmd+Z
- Ctrl/Cmd+Shift+Z
- Ctrl/Cmd+A
- similar text-editing shortcuts

reset current expansion context.

Do not inspect clipboard contents to reconstruct token state in v1.

---

## 21. Import/export implementation placement

Suggested shared parser:

```
Engine/TextExpansionIO.h
Engine/TextExpansionIO.cpp
```

Responsibilities:

- JSON serialization/deserialization independent of platform file paths
- OpenKey/UniKey text import/export
- legacy OpenKey binary parser

Platform layers handle:

- file dialogs
- actual paths
- Registry/UserDefaults
- filesystem access if preferred

If adding JSON dependencies, avoid a large third-party dependency only for this feature.

A small purpose-built serializer/parser for this narrow schema is acceptable, provided escaping is correct and tested.

If the repository already contains a JSON library suitable for shared C++, reuse it.

---

## 22. Build integration

### Windows

Update:

```
Windows/App/CMakeLists.txt
```

to compile:

```
../../Engine/TextExpansion.cpp
../../Engine/TextExpansionIO.cpp
```

or include them in the shared `vikey_engine` target if that is the existing architectural convention.

Do not duplicate compilation inconsistently between targets.

Expected release build remains compatible with the repository's current CMake workflow.

### macOS

Update:

```
project.yml
```

if new shared source files are not already globbed automatically.

Regenerate project only if repository workflow requires it.

Do not hand-edit generated Xcode project files if `project.yml` is the source of truth.

---

## 23. Tests

Before writing feature code, locate current engine/simulator test structure and extend it.

### 23.1 Shared unit tests

Test manager/session independent of OS hooks.

Minimum:

```
dc -> được
ss -> Sun Santa
ph -> phụ huynh
```

Exact matching:

```
dc     => match
abcdc  => no match
d      => no match
```

Unicode:

```
trigger:  dc
replace:  được
```

Emoji:

```
heart -> ❤️
```

Multiline:

```
sig -> "Harry Tran\nSun Santa"
```

Long replacement > 32 characters.

Duplicate add/update.

Delete entry.

Malformed import.

JSON round trip.

OpenKey text round trip.

Legacy OpenKey binary bounds checking.

### 23.2 Boundary tests

```
dc + Space       -> được 
dc + Enter       -> được\n
dc + Tab         -> được<Tab>
dc + ,           -> được,
dc + .           -> được.
dc + ;           -> được;
dc + :           -> được:
dc + !           -> được!
dc + ?           -> được?
```

### 23.3 Auto Caps tests

When off:

```
dc -> được
Dc -> no match or exact configured behavior
DC -> no match or exact configured behavior
```

When on:

```
dc -> được
Dc -> Được
DC -> ĐƯỢC
```

Choose one explicit behavior and make Windows/macOS identical.

### 23.4 Vietnamese-engine integration

Test while in Vietnamese mode:

```
dc + Space -> được 
```

Then continue typing normal Vietnamese immediately.

Test a trigger containing Vietnamese characters if supported.

Test after:

- `dd`
- `aw`
- `ow`
- tone keys
- invalid-spelling restore

### 23.5 English mode

With "UseMacroInEnglishMode" ON:

```
ss + Space -> Sun Santa 
```

With it OFF:

```
ss + Space -> ss 
```

### 23.6 Auto Switch integration

Required scenarios:

```
dc + Space + normal English word
dc + Space + normal Vietnamese word
English auto-switch immediately before expansion
English auto-switch immediately after expansion
manual EN/VI switch then expansion
```

Expansion must never accidentally toggle language.

### 23.7 Backspace

```
d c Backspace -> session becomes d
dc Space Backspace -> removes boundary normally
```

Do not accidentally invoke auto-fix undo for an expansion.

### 23.8 Recursion

```
a -> b
b -> c
```

Typing:

```
a Space
```

must result in:

```
b 
```

not:

```
c 
```

### 23.9 Context reset

After trigger text, then:

- mouse click
- Left
- Right
- Home
- End
- Ctrl/Cmd+V
- app switch

a later Space must NOT expand stale text.

### 23.10 Secure input

Password field:

```
dc + Space
```

must remain literal.

No expansion and no stored history.

---

## 24. Manual application test matrix

At minimum test on Windows:

- Notepad
- Chrome
- Microsoft Word
- Microsoft Excel
- VS Code
- Zalo
- Messenger/Web Messenger
- Windows Terminal
- password field in a browser

At minimum test on macOS:

- TextEdit
- Safari
- Chrome
- Microsoft Word
- Microsoft Excel
- VS Code
- Zalo if available
- Notes
- Terminal
- password field / secure input

Record any app-specific replacement workaround needed.

Do not add broad hacks unless the failure is reproducible.

---

## 25. Performance requirements

Keyboard hooks/event taps are latency-sensitive.

Per keystroke:

- no disk access
- no JSON parsing
- no Registry access
- no UserDefaults access
- no O(n) scan through a large entry list

Load entries into an in-memory hash map.

Boundary lookup should be approximately O(1) plus trigger string hashing.

File writes happen only on settings changes/import/export.

---

## 26. Thread-safety

Settings UI may update entries while keyboard handling is active.

Use one of:

- immutable snapshot replacement
- lightweight mutex around manager replacement
- dispatch to the hook's expected thread

Do not mutate a vector/map in place while the keyboard hook can read it.

Preferred approach:

1. settings constructs a complete new entry snapshot
2. core swaps it atomically / under a short lock
3. keyboard lookup reads stable data

---

## 27. Error handling

Storage load failure:

- do not crash
- start with empty list
- preserve original corrupt file if possible
- log a concise diagnostic

Import failure:

- do not partially corrupt current entries
- parse to temporary collection first
- merge only after successful parse / user confirmation

Export failure:

- report error
- do not alter current entries

---

## 28. UX details

Suggested Vietnamese labels:

```
Gõ tắt
Bật gõ tắt
Gõ tắt trong chế độ tiếng Anh
Tự viết hoa
Từ gõ tắt
Nội dung
Thêm
Sửa
Xoá
Nhập
Xuất
Tìm kiếm
```

Validation messages:

```
Từ gõ tắt không được để trống.
Nội dung thay thế không được để trống.
Từ gõ tắt này đã tồn tại.
```

When deleting, avoid confirmation for a single trivial entry if undo/UI design makes deletion obvious; otherwise ask confirmation for destructive bulk actions.

---

## 29. Suggested implementation phases

### Phase 0 — Safety baseline

Before feature changes:

- verify Windows build
- verify macOS build if environment supports it
- run existing tests
- record baseline failures if any

Do not fix unrelated baseline failures in this feature branch unless they block implementation.

Commit:

```
chore(text-expansion): record baseline and test plan
```

Optional if no source change is needed.

---

### Phase 1 — Shared model + matcher + session

Create:

```
Engine/TextExpansion.h
Engine/TextExpansion.cpp
```

Implement:

- entries
- exact lookup
- auto-caps matching
- session synchronization primitives
- no platform code

Add unit tests.

Commit:

```
feat(text-expansion): add shared matcher and session
```

Acceptance:

- shared tests pass
- no behavior change when feature is unused

---

### Phase 2 — Shared persistence/import/export

Create:

```
Engine/TextExpansionIO.h
Engine/TextExpansionIO.cpp
```

Implement:

- JSON v1
- OpenKey/UniKey import/export
- legacy OpenKey binary decoding

Add tests.

Commit:

```
feat(text-expansion): add storage and OpenKey compatibility
```

---

### Phase 3 — Windows core integration

Modify:

```
Windows/App/Core.cpp
Windows/App/CMakeLists.txt
```

Potentially add:

```
Windows/App/TextExpansionStore.cpp
Windows/App/TextExpansionStore.h
```

Implement:

- load entries at startup
- reload API
- token tracking
- boundary expansion
- secure field skip
- context resets
- tagged injected output

Do not build settings UI yet if that complicates debugging; seed test entries through storage/test helper if needed.

Commit:

```
feat(windows): integrate text expansion into keyboard core
```

Acceptance:

- basic expansion works in Windows
- auto-switch still works
- no recursion
- password fields excluded

---

### Phase 4 — Windows UI + OpenKey migration

Modify:

```
Windows/App/Settings.cpp
Windows/App/Settings.h
Windows/App/Prefs.cpp
Windows/App/Prefs.h
```

Implement full UI and one-time OpenKey migration.

Commit:

```
feat(windows): add text expansion settings and OpenKey migration
```

---

### Phase 5 — macOS core integration

Modify:

```
App/Core/VikeyCore.mm
App/Core/VikeyCore.h
project.yml
```

Potentially add a small macOS storage bridge.

Match Windows behavior exactly.

Commit:

```
feat(macos): integrate text expansion into keyboard core
```

---

### Phase 6 — macOS UI

Add:

```
App/Model/TextExpansionStore.swift
App/Views/TextExpansionView.swift
```

Modify:

```
App/Model/Preferences.swift
App/Views/MainView.swift
```

Commit:

```
feat(macos): add text expansion settings UI
```

---

### Phase 7 — Regression + polish

Run:

- shared tests
- engine tests
- Windows build
- macOS build
- manual compatibility matrix

Fix only feature-related regressions.

Commit:

```
test(text-expansion): cover auto switch and platform edge cases
```

---

## 30. Required acceptance criteria

Feature is not complete until all of the following are true.

### Basic

- [ ] Windows: `dc + Space -> được `
- [ ] macOS: `dc + Space -> được `
- [ ] Windows: `ss + Space -> Sun Santa `
- [ ] macOS: `ss + Space -> Sun Santa `
- [ ] punctuation boundary preserved
- [ ] Enter preserved
- [ ] Tab preserved
- [ ] Unicode works
- [ ] emoji works
- [ ] multiline works
- [ ] replacement > 32 chars works

### Modes

- [ ] expansion works in Vietnamese mode
- [ ] expansion works in English mode when enabled
- [ ] expansion does not work in English mode when disabled
- [ ] expansion does not unexpectedly change language

### Existing Vikey behavior

- [ ] normal Telex works
- [ ] Simple Telex 1 works
- [ ] Simple Telex 2 works
- [ ] spelling check works
- [ ] invalid-word restore works
- [ ] teencode works
- [ ] Auto Switch EN -> VI works
- [ ] Auto Switch VI -> EN works
- [ ] AutoFixUndo still works for auto-switch corrections
- [ ] navigation behavior is not regressed
- [ ] smart per-app switching is not regressed

### Safety

- [ ] no expansion in password field
- [ ] no recursive expansion
- [ ] stale token is cleared after click/navigation/app change
- [ ] malformed JSON does not crash
- [ ] malformed OpenKey import does not crash
- [ ] corrupted legacy registry macro data does not crash

### UI/data

- [ ] add/edit/delete works on Windows
- [ ] add/edit/delete works on macOS
- [ ] search works
- [ ] import works
- [ ] export works
- [ ] settings persist after restart
- [ ] changes take effect without restart

---

## 31. Do NOT do these things

- Do not wholesale copy old OpenKey `Macro.cpp` into current Vikey `Engine.cpp`.
- Do not re-add unbounded `std::vector` fields to `vKeyHookState` just to imitate OpenKey.
- Do not create a second keyboard hook/event tap.
- Do not use clipboard paste as the default expansion mechanism.
- Do not perform filesystem/Registry/UserDefaults reads on every key.
- Do not limit replacement to `MAX_BUFF`.
- Do not recursively process Vikey-generated synthetic events.
- Do not attempt to read surrounding arbitrary application text around the cursor in v1.
- Do not silently expand inside password fields.
- Do not change unrelated auto-switch heuristics while implementing this feature.
- Do not hand-edit generated Xcode files if `project.yml` is authoritative.
- Do not merge directly to `main` until tests pass.

---

## 32. Guidance for Codex while editing

For each phase:

1. inspect current code before modifying it
2. identify the smallest integration points
3. keep behavior unchanged when text expansion is disabled
4. add tests before or alongside feature code
5. build/test after each meaningful step
6. make small commits with the suggested commit messages
7. if current source differs from assumptions in this plan, adapt to the current architecture instead of forcing this exact pseudocode
8. document any architectural deviation in this file or a short implementation note
9. do not delete existing compatibility logic without a regression test
10. prefer a safe context reset over guessing when the visible token cannot be known reliably

---

## 33. Recommended initial sample entries for development only

Do not ship these as mandatory defaults unless product decision says so.

```
dc -> được
ph -> phụ huynh
ss -> Sun Santa
lh -> liên hệ
heart -> ❤️
```

They are useful for manual testing.

---

## 34. Upstream maintenance strategy

This fork should stay easy to rebase onto `tonyminibot/Vikey`.

Therefore:

- keep shared text-expansion code in dedicated files
- keep changes to `Engine.cpp` minimal or zero if possible
- isolate platform integration in clear sections
- avoid broad formatting changes
- avoid renaming unrelated symbols
- avoid refactoring large auto-switch areas as part of this feature

Before rebasing future upstream versions:

```bash
git fetch upstream
git checkout feature/text-expansion
git rebase upstream/main
```

Resolve conflicts by preserving upstream engine behavior first, then reapplying the small text-expansion hooks.

---

## 35. License

Vikey and OpenKey are GPL-3.0-derived projects.

Any distributed modified Vikey binary must continue complying with GPL-3.0 requirements, including corresponding source availability.

Keep existing license notices and acknowledgements intact.

If code is directly adapted from OpenKey, retain appropriate attribution.

---

## 36. Definition of done

The feature is done when a normal user can install the fork on Windows or macOS, open **Gõ tắt**, create:

```
dc -> được
```

then type:

```
dc + Space
```

in a normal text editor and reliably receive:

```
được 
```

while all of Vikey's existing Vietnamese typing and automatic English/Vietnamese switching continue to behave as before.

The implementation should feel like a native Vikey feature, not a second text-injection application bolted onto Vikey.
