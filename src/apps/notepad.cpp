#include "notepad.h"
#include "core/strings.h"
#include "fs/virtual_fs.h"
#include "misc/cpp/imgui_stdlib.h"
#include "os/shell.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <ctime>

static const ImVec4 ERROR_COLOR(1.0f, 0.45f, 0.45f, 1.0f);
static const ImVec4 WARN_COLOR(1.0f, 0.75f, 0.30f, 1.0f);

static std::string Timestamp() {
    std::time_t now = std::time(nullptr);
    std::tm     lt;
#ifdef _WIN32
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%I:%M %p %m/%d/%Y", &lt);
    return buf;
}

// ---------------------------------------------------------------------------
// Documents
// ---------------------------------------------------------------------------

std::string Notepad::Title() const {
    for (const Document& doc : docs)
        if (doc.id == activeId)
            return (doc.Dirty() ? "*" : "") + doc.name + " - Notepad";
    return "Notepad";
}

bool Notepad::HasUnsavedChanges() const {
    return std::any_of(docs.begin(), docs.end(), [](const Document& d) { return d.Dirty(); });
}

Notepad::Document* Notepad::Find(int id) {
    for (Document& doc : docs)
        if (doc.id == id) return &doc;
    return nullptr;
}

Notepad::Document* Notepad::Active() {
    return Find(activeId);
}

void Notepad::OnOpen(Shell&) {
    if (docs.empty())
        NewDocument();
}

void Notepad::OnClose(Shell&) {
    docs.clear();
    activeId      = -1;
    untitledCount = 0;
}

bool Notepad::OnCloseRequest(Shell&) {
    if (!HasUnsavedChanges())
        return true;
    openExitConfirm = true;
    return false;
}

void Notepad::NewDocument() {
    Document doc;
    doc.id   = nextId++;
    doc.name = "Untitled " + std::to_string(++untitledCount);
    docs.push_back(doc);
    selectRequest = doc.id;
}

void Notepad::OpenFile(Shell& shell, const std::string& path) {
    FsNode* node = shell.fs.Resolve(path);
    if (!node || node->isDir) {
        shell.Notify("Notepad", "Can't open '" + path + "'.", ToastKind::Error);
        return;
    }

    std::string full = VirtualFS::PathOf(node);
    for (const Document& doc : docs) {
        if (EqualsNoCase(doc.path, full)) {
            selectRequest = doc.id;
            return;
        }
    }

    // Replace the blank document Notepad starts with.
    if (docs.size() == 1 && docs[0].path.empty() && docs[0].text.empty())
        docs.clear();

    Document doc;
    doc.id        = nextId++;
    doc.path      = full;
    doc.name      = node->name;
    doc.text      = node->content;
    doc.savedText = node->content;
    doc.readOnly  = node->readOnly;
    docs.push_back(doc);
    selectRequest = doc.id;
}

void Notepad::CloseDocument(int id) {
    docs.erase(std::remove_if(docs.begin(), docs.end(), [id](const Document& d) { return d.id == id; }),
               docs.end());
    if (activeId == id)
        activeId = -1;
}

void Notepad::RequestCloseDocument(int id) {
    Document* doc = Find(id);
    if (!doc) return;
    if (doc->Dirty()) {
        closeId          = id;
        openCloseConfirm = true;
    } else {
        CloseDocument(id);
    }
}

bool Notepad::Save(Shell& shell, Document& doc) {
    if (doc.path.empty() || doc.readOnly) {
        BeginSaveAs(doc, false);
        return false;
    }
    std::string error;
    if (!SaveTo(shell, doc, doc.path, &error)) {
        shell.Notify("Notepad", error, ToastKind::Error);
        return false;
    }
    return true;
}

bool Notepad::SaveTo(Shell& shell, Document& doc, const std::string& path, std::string* error) {
    VirtualFS&  fs     = shell.fs;
    std::string target = Trim(path);
    if (target.empty()) {
        *error = "Please enter a file name.";
        return false;
    }
    if (target[0] != '/')
        target = VirtualFS::Join("/Documents", target);

    FsNode* node = fs.Resolve(target);
    if (node) {
        if (node->isDir) {
            *error = "'" + target + "' is a folder.";
            return false;
        }
        if (!fs.WriteFile(node, doc.text)) {
            *error = fs.LastError();
            return false;
        }
    } else {
        FsNode* parent = fs.Resolve(VirtualFS::ParentOf(target));
        if (!parent || !parent->isDir) {
            *error = "The folder '" + VirtualFS::ParentOf(target) + "' doesn't exist.";
            return false;
        }
        node = fs.NewFile(parent, VirtualFS::NameOf(target), doc.text);
        if (!node) {
            *error = fs.LastError();
            return false;
        }
    }

    doc.path      = VirtualFS::PathOf(node);
    doc.name      = node->name;
    doc.savedText = doc.text;
    doc.readOnly  = false;
    return true;
}

bool Notepad::SaveOrAutoName(Shell& shell, Document& doc) {
    std::string path = doc.path;
    if (path.empty() || doc.readOnly) {
        // Untitled (or read-only) documents go to /Documents under a free name.
        std::string wanted = doc.path.empty() ? doc.name + ".txt" : doc.name;
        path = VirtualFS::Join("/Documents", shell.fs.UniqueName(shell.fs.Resolve("/Documents"), wanted));
    }
    std::string error;
    if (!SaveTo(shell, doc, path, &error)) {
        shell.Notify("Notepad", error, ToastKind::Error);
        return false;
    }
    shell.Notify("Notepad", "Saved " + doc.path + ".", ToastKind::Success);
    return true;
}

void Notepad::BeginSaveAs(const Document& doc, bool closeAfter) {
    std::string suggestion = doc.path;
    if (doc.path.empty())
        suggestion = "/Documents/" + doc.name + ".txt";
    else if (doc.readOnly)
        suggestion = "/Documents/" + doc.name;

    snprintf(pathBuf, sizeof(pathBuf), "%s", suggestion.c_str());
    saveAsId         = doc.id;
    closeAfterSaveAs = closeAfter;
    dialogError.clear();
    openSaveAs = true;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

int Notepad::TextCallback(ImGuiInputTextCallbackData* data) {
    Notepad* self = static_cast<Notepad*>(data->UserData);
    if (data->EventFlag != ImGuiInputTextFlags_CallbackAlways)
        return 0;

    // F5 inserts the time and date at the cursor, like Windows Notepad.
    if (self->insertTimestamp && !(data->Flags & ImGuiInputTextFlags_ReadOnly))
        data->InsertChars(data->CursorPos, Timestamp().c_str());
    self->insertTimestamp = false;

    int line = 1, col = 1;
    for (int i = 0; i < data->CursorPos && i < data->BufTextLen; ++i) {
        if (data->Buf[i] == '\n') { ++line; col = 1; }
        else                      { ++col; }
    }
    self->cursorLine = line;
    self->cursorCol  = col;
    return 0;
}

void Notepad::Render(Shell& shell) {
    RenderMenuBar(shell);

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
            if (Document* doc = Active())
                Save(shell, *doc);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false))
            NewDocument();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
            openFileDialog = true;
        if (io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F5, false))
            insertTimestamp = true;
    }

    if (docs.empty()) {
        ImGui::TextDisabled("No documents are open.");
        if (ImGui::Button("New document")) NewDocument();
        ImGui::SameLine();
        if (ImGui::Button("Open...")) openFileDialog = true;
        RenderDialogs(shell);
        return;
    }

    const float statusH      = ImGui::GetTextLineHeightWithSpacing() + 2.0f;
    int         closeRequest = -1;
    if (ImGui::BeginTabBar("##docs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs |
                                     ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (Document& doc : docs) {
            ImGuiTabItemFlags flags = doc.Dirty() ? ImGuiTabItemFlags_UnsavedDocument : 0;
            if (selectRequest == doc.id)
                flags |= ImGuiTabItemFlags_SetSelected;

            bool        keepOpen = true;
            std::string label    = doc.name + "###doc" + std::to_string(doc.id);
            if (ImGui::BeginTabItem(label.c_str(), &keepOpen, flags)) {
                activeId = doc.id;
                ImGuiInputTextFlags textFlags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways;
                if (doc.readOnly)
                    textFlags |= ImGuiInputTextFlags_ReadOnly;
                ImGui::PushFont(shell.fonts.mono);
                ImGui::InputTextMultiline("##text", &doc.text, ImVec2(-FLT_MIN, -statusH), textFlags,
                                          &Notepad::TextCallback, this);
                ImGui::PopFont();
                ImGui::EndTabItem();
            }
            if (!keepOpen)
                closeRequest = doc.id;
        }
        selectRequest = -1;
        ImGui::EndTabBar();
    }
    if (closeRequest >= 0)
        RequestCloseDocument(closeRequest);

    RenderStatusBar();
    RenderDialogs(shell);
}

void Notepad::RenderMenuBar(Shell& shell) {
    if (!ImGui::BeginMenuBar())
        return;

    Document* doc = Active();
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New", "Ctrl+N"))                          NewDocument();
        if (ImGui::MenuItem("Open...", "Ctrl+O"))                      openFileDialog = true;
        if (ImGui::MenuItem("Save", "Ctrl+S", false, doc != nullptr))  Save(shell, *doc);
        if (ImGui::MenuItem("Save As...", nullptr, false, doc != nullptr)) BeginSaveAs(*doc, false);
        ImGui::Separator();
        if (ImGui::MenuItem("Close tab", nullptr, false, doc != nullptr)) RequestCloseDocument(doc->id);
        if (ImGui::MenuItem("Exit"))                                   shell.CloseApp(AppID::Notepad);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        bool editable = doc && !doc->readOnly;
        if (ImGui::MenuItem("Time/Date", "F5", false, editable)) doc->text += Timestamp();
        if (ImGui::MenuItem("Clear all", nullptr, false, editable)) doc->text.clear();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About Notepad"))
            shell.Notify("Notepad", "CSOPESY Notepad v1.0 - a plain text editor for the virtual file system.");
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

void Notepad::RenderStatusBar() {
    const Document* doc = Active();
    if (!doc)
        return;
    ImGui::TextDisabled("Ln %d, Col %d", cursorLine, cursorCol);
    ImGui::SameLine();
    ImGui::TextDisabled("|  %d characters", (int)doc->text.size());
    ImGui::SameLine();
    ImGui::TextDisabled("|  %s", doc->path.empty() ? "Not saved yet" : doc->path.c_str());
    if (doc->readOnly) {
        ImGui::SameLine();
        ImGui::TextColored(WARN_COLOR, "|  Read-only");
    }
}

void Notepad::RenderDialogs(Shell& shell) {
    // --- Save As ---
    if (openSaveAs) {
        ImGui::OpenPopup("Save As");
        openSaveAs = false;
    }
    if (ImGui::BeginPopupModal("Save As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        Document* doc = Find(saveAsId);
        if (!doc) {
            ImGui::CloseCurrentPopup();
        } else {
            ImGui::TextUnformatted("Save as (e.g. /Documents/notes.txt):");
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(380.0f);
            bool submit = ImGui::InputText("##path", pathBuf, sizeof(pathBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            if (!dialogError.empty())
                ImGui::TextColored(ERROR_COLOR, "%s", dialogError.c_str());

            submit |= ImGui::Button("Save", ImVec2(120, 0));
            ImGui::SameLine();
            bool cancel = ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape);

            if (submit && SaveTo(shell, *doc, pathBuf, &dialogError)) {
                shell.Notify("Notepad", "Saved " + doc->path + ".", ToastKind::Success);
                if (closeAfterSaveAs)
                    CloseDocument(saveAsId);
                ImGui::CloseCurrentPopup();
            } else if (cancel) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    // --- Open ---
    if (openFileDialog) {
        ImGui::OpenPopup("Open File");
        openFileDialog = false;
        filterBuf[0]   = '\0';
        openSelection.clear();
    }
    ImGui::SetNextWindowSize(ImVec2(460, 360), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Open File")) {
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##filter", "Filter text files by name", filterBuf, sizeof(filterBuf));

        std::vector<FsNode*> all;
        shell.fs.Find(shell.fs.Root(), "", all);

        bool opened = false;
        ImGui::BeginChild("##files", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_Border);
        for (FsNode* node : all) {
            if (VirtualFS::KindOf(node) != FileKind::Text || !ContainsNoCase(node->name, filterBuf))
                continue;
            std::string path = VirtualFS::PathOf(node);
            ImGui::PushID(path.c_str());
            if (IconSelectable(path.c_str(), Icon::TextFile, openSelection == path, ImGuiSelectableFlags_AllowDoubleClick)) {
                openSelection = path;
                opened = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        ImGui::BeginDisabled(openSelection.empty());
        opened |= ImGui::Button("Open", ImVec2(120, 0));
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();

        if (opened && !openSelection.empty()) {
            OpenFile(shell, openSelection);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // --- Closing a tab with unsaved changes ---
    if (openCloseConfirm) {
        ImGui::OpenPopup("Unsaved changes##tab");
        openCloseConfirm = false;
    }
    if (ImGui::BeginPopupModal("Unsaved changes##tab", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        Document* doc = Find(closeId);
        if (!doc) {
            ImGui::CloseCurrentPopup();
        } else {
            ImGui::Text("Do you want to save changes to '%s'?", doc->name.c_str());
            ImGui::Spacing();
            if (ImGui::Button("Save", ImVec2(110, 0))) {
                if (doc->path.empty() || doc->readOnly)
                    BeginSaveAs(*doc, true);
                else if (Save(shell, *doc))
                    CloseDocument(closeId);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't save", ImVec2(110, 0))) {
                CloseDocument(closeId);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(110, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // --- Closing the whole window with unsaved changes ---
    if (openExitConfirm) {
        ImGui::OpenPopup("Unsaved changes##exit");
        openExitConfirm = false;
    }
    if (ImGui::BeginPopupModal("Unsaved changes##exit", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        int dirty = (int)std::count_if(docs.begin(), docs.end(), [](const Document& d) { return d.Dirty(); });
        ImGui::Text("%d document%s unsaved changes.", dirty, dirty == 1 ? " has" : "s have");
        ImGui::TextDisabled("Untitled documents are saved to /Documents.");
        ImGui::Spacing();

        bool closeWindow = false;
        if (ImGui::Button("Save all", ImVec2(110, 0))) {
            bool allSaved = true;
            for (Document& doc : docs)
                if (doc.Dirty())
                    allSaved &= SaveOrAutoName(shell, doc);
            closeWindow = allSaved;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't save", ImVec2(110, 0))) {
            closeWindow = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(110, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();

        if (closeWindow)
            shell.CloseApp(AppID::Notepad, true);
    }
}
