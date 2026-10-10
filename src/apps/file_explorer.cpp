#include "file_explorer.h"
#include "core/strings.h"
#include "fs/virtual_fs.h"
#include "os/shell.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>

enum ColumnId { COL_NAME, COL_FOLDER, COL_DATE, COL_TYPE, COL_SIZE };

static const ImVec4 ERROR_COLOR(1.0f, 0.45f, 0.45f, 1.0f);
static const ImVec4 WARN_COLOR(1.0f, 0.75f, 0.30f, 1.0f);

std::string FileExplorer::Title() const {
    return (cwd == "/" ? std::string("This PC") : VirtualFS::NameOf(cwd)) + " - File Explorer";
}

// The selected item, or nullptr. (An empty path would resolve to the
// current folder, which is not "selected".)
FsNode* FileExplorer::SelectedNode(Shell& shell) const {
    return selected.empty() ? nullptr : shell.fs.Resolve(selected);
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

void FileExplorer::Navigate(const std::string& folderPath) {
    if (folderPath != cwd) {
        backStack.push_back(cwd);
        forwardStack.clear();
        cwd = folderPath;
    }
    selected.clear();
    searchBuf[0] = '\0';
    syncTree = true;
}

void FileExplorer::Reveal(const std::string& filePath) {
    Navigate(VirtualFS::ParentOf(filePath));
    selected = filePath;
}

void FileExplorer::GoBack() {
    if (backStack.empty()) return;
    forwardStack.push_back(cwd);
    cwd = backStack.back();
    backStack.pop_back();
    selected.clear();
    syncTree = true;
}

void FileExplorer::GoForward() {
    if (forwardStack.empty()) return;
    backStack.push_back(cwd);
    cwd = forwardStack.back();
    forwardStack.pop_back();
    selected.clear();
    syncTree = true;
}

void FileExplorer::GoUp() {
    if (cwd == "/") return;
    std::string from = cwd;
    Navigate(VirtualFS::ParentOf(cwd));
    selected = from;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void FileExplorer::Render(Shell& shell) {
    FsNode* dir = shell.fs.Resolve(cwd);
    if (!dir || !dir->isDir) {
        // The folder vanished (e.g. deleted from the Terminal): fall back to This PC.
        cwd = "/";
        dir = shell.fs.Root();
    }
    if (!selected.empty() && !SelectedNode(shell))
        selected.clear();

    RenderToolbar();
    RenderCommandBar(shell, dir);

    const float statusH  = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
    const float navW     = 180.0f;
    const float previewW = 230.0f;
    const float spacing  = ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginChild("##nav", ImVec2(navW, -statusH), ImGuiChildFlags_Border);
    RenderNavPane(shell);
    ImGui::EndChild();

    ImGui::SameLine();
    size_t itemCount = 0;
    ImGui::BeginChild("##content", ImVec2(-(previewW + spacing), -statusH), ImGuiChildFlags_Border);
    RenderContents(shell, dir, &itemCount);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##preview", ImVec2(0, -statusH), ImGuiChildFlags_Border);
    RenderPreview(shell);
    ImGui::EndChild();

    RenderStatusBar(shell, itemCount);
    HandleShortcuts(shell, dir);
    RenderDialogs(shell);
}

void FileExplorer::RenderToolbar() {
    ImGui::BeginDisabled(backStack.empty());
    if (ImGui::ArrowButton("##back", ImGuiDir_Left)) GoBack();
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Back");

    ImGui::SameLine();
    ImGui::BeginDisabled(forwardStack.empty());
    if (ImGui::ArrowButton("##forward", ImGuiDir_Right)) GoForward();
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Forward");

    ImGui::SameLine();
    ImGui::BeginDisabled(cwd == "/");
    if (ImGui::ArrowButton("##up", ImGuiDir_Up)) GoUp();
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Up to the parent folder");

    // Address bar made of clickable breadcrumbs.
    const float searchW = 200.0f;
    ImGui::SameLine();
    float  barW   = ImGui::GetContentRegionAvail().x - searchW - ImGui::GetStyle().ItemSpacing.x;
    ImVec2 barPos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(barPos, barPos + ImVec2(barW, ImGui::GetFrameHeight()),
        ImGui::GetColorU32(ImGuiCol_FrameBg), ImGui::GetStyle().FrameRounding);

    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
    if (ImGui::Button("This PC"))
        Navigate("/");
    std::string path = "/";
    size_t start = 1;
    while (start < cwd.size()) {
        size_t end = cwd.find('/', start);
        if (end == std::string::npos) end = cwd.size();
        std::string part = cwd.substr(start, end - start);
        path  = VirtualFS::Join(path, part);
        start = end + 1;

        ImGui::SameLine();
        ImGui::TextDisabled(">");
        ImGui::SameLine();
        ImGui::PushID(path.c_str());
        if (ImGui::Button(part.c_str()))
            Navigate(path);
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SameLine();
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > searchW)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - searchW);
    ImGui::SetNextItemWidth(searchW);
    std::string hint = "Search " + (cwd == "/" ? std::string("This PC") : VirtualFS::NameOf(cwd));
    ImGui::InputTextWithHint("##search", hint.c_str(), searchBuf, sizeof(searchBuf));
}

void FileExplorer::RenderCommandBar(Shell& shell, FsNode* dir) {
    FsNode* sel = SelectedNode(shell);

    if (ImGui::Button("+ New folder")) CreateItem(shell, dir, true);
    ImGui::SameLine();
    if (ImGui::Button("+ New file")) CreateItem(shell, dir, false);

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::BeginDisabled(!sel);
    if (ImGui::Button("Cut")) SetClipboard(shell, sel, true);
    ImGui::SameLine();
    if (ImGui::Button("Copy")) SetClipboard(shell, sel, false);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(clipboardPath.empty());
    if (ImGui::Button("Paste")) Paste(shell, dir);
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::BeginDisabled(!sel);
    if (ImGui::Button("Open")) Open(shell, sel);
    ImGui::SameLine();
    if (ImGui::Button("Rename")) BeginRename(shell, sel);
    ImGui::SameLine();
    if (ImGui::Button("Delete")) BeginDelete(shell, sel);
    ImGui::EndDisabled();

    ImGui::SameLine();
    const float toggleW = 150.0f;
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > toggleW)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - toggleW);
    if (ImGui::RadioButton("Details", !iconView)) iconView = false;
    ImGui::SameLine();
    if (ImGui::RadioButton("Icons", iconView)) iconView = true;
}

void FileExplorer::RenderNavPane(Shell& shell) {
    ImGui::TextDisabled("Quick access");
    static const char* QUICK_ACCESS[] = { "/Desktop", "/Documents", "/Downloads", "/Pictures" };
    for (const char* path : QUICK_ACCESS) {
        ImGui::PushID(path);
        if (IconSelectable(VirtualFS::NameOf(path).c_str(), Icon::Folder, cwd == path))
            Navigate(path);
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    RenderNavTree(shell.fs.Root());
    syncTree = false;
}

void FileExplorer::RenderNavTree(FsNode* node) {
    std::string path = VirtualFS::PathOf(node);
    bool hasSubfolders = std::any_of(node->children.begin(), node->children.end(),
                                     [](const std::unique_ptr<FsNode>& c) { return c->isDir; });

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasSubfolders) flags |= ImGuiTreeNodeFlags_Leaf;
    if (path == cwd)    flags |= ImGuiTreeNodeFlags_Selected;
    if (!node->parent)  flags |= ImGuiTreeNodeFlags_DefaultOpen;

    // After navigating, expand the branch that leads to the current folder.
    if (syncTree && (path == "/" || cwd.rfind(path + "/", 0) == 0))
        ImGui::SetNextItemOpen(true);

    bool open = ImGui::TreeNodeEx(path.c_str(), flags, "%s",
                                  node->parent ? node->name.c_str() : "This PC");
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        Navigate(path);

    if (open) {
        for (auto& child : node->children)
            if (child->isDir)
                RenderNavTree(child.get());
        ImGui::TreePop();
    }
}

void FileExplorer::RenderContents(Shell& shell, FsNode* dir, size_t* itemCount) {
    std::string query     = Trim(searchBuf);
    bool        searching = !query.empty();

    std::vector<FsNode*> items;
    if (searching)
        shell.fs.Find(dir, query, items);
    else
        for (auto& child : dir->children)
            items.push_back(child.get());
    *itemCount = items.size();

    if (items.empty())
        ImGui::TextDisabled(searching ? "No items match your search." : "This folder is empty.");
    else if (iconView)
        RenderIcons(shell, items);
    else
        RenderDetails(shell, items, searching);

    // Clicking empty space clears the selection; right-clicking it opens the folder menu.
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !ImGui::IsAnyItemHovered()) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            selected.clear();
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            ImGui::OpenPopup("##background");
    }
    if (ImGui::BeginPopup("##background")) {
        if (ImGui::MenuItem("New folder"))        CreateItem(shell, dir, true);
        if (ImGui::MenuItem("New text document")) CreateItem(shell, dir, false);
        ImGui::Separator();
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, !clipboardPath.empty()))
            Paste(shell, dir);
        ImGui::Separator();
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Details", nullptr, !iconView)) iconView = false;
            if (ImGui::MenuItem("Icons", nullptr, iconView))    iconView = true;
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
}

void FileExplorer::RenderDetails(Shell& shell, std::vector<FsNode*>& items, bool searching) {
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("##files", searching ? 5 : 4, flags))
        return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 0.0f, COL_NAME);
    if (searching)
        ImGui::TableSetupColumn("Folder", ImGuiTableColumnFlags_WidthStretch, 0.0f, COL_FOLDER);
    ImGui::TableSetupColumn("Date modified", ImGuiTableColumnFlags_WidthFixed, 140.0f, COL_DATE);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 120.0f, COL_TYPE);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 75.0f, COL_SIZE);
    ImGui::TableHeadersRow();

    ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
    if (specs && specs->SpecsCount > 0) {
        const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
        const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
        std::stable_sort(items.begin(), items.end(), [&](const FsNode* a, const FsNode* b) {
            if (a->isDir != b->isDir) return a->isDir;   // folders always come first
            int cmp = 0;
            switch (spec.ColumnUserID) {
            case COL_DATE:   cmp = (a->modified < b->modified) ? -1 : (a->modified > b->modified ? 1 : 0); break;
            case COL_TYPE:   cmp = VirtualFS::TypeName(a).compare(VirtualFS::TypeName(b)); break;
            case COL_FOLDER: cmp = ToLower(VirtualFS::PathOf(a->parent)).compare(ToLower(VirtualFS::PathOf(b->parent))); break;
            case COL_SIZE: {
                size_t sa = VirtualFS::SizeOf(a), sb = VirtualFS::SizeOf(b);
                cmp = (sa < sb) ? -1 : (sa > sb ? 1 : 0);
                break;
            }
            default:         cmp = ToLower(a->name).compare(ToLower(b->name)); break;
            }
            return ascending ? cmp < 0 : cmp > 0;
        });
    }

    for (FsNode* node : items) {
        std::string path = VirtualFS::PathOf(node);
        ImGui::PushID(path.c_str());
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        if (IconSelectable(node->name.c_str(), IconForNode(node), path == selected,
                           ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
            selected = path;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                Open(shell, node);
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            selected = path;
        if (ImGui::BeginPopupContextItem("##item")) {
            ItemContextMenu(shell, node);
            ImGui::EndPopup();
        }

        if (searching) {
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", VirtualFS::PathOf(node->parent).c_str());
        }
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", VirtualFS::FormatTime(node->modified).c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(VirtualFS::TypeName(node).c_str());
        ImGui::TableNextColumn();
        if (!node->isDir)
            ImGui::TextUnformatted(VirtualFS::FormatSize(VirtualFS::SizeOf(node)).c_str());

        ImGui::PopID();
    }
    ImGui::EndTable();
}

void FileExplorer::RenderIcons(Shell& shell, const std::vector<FsNode*>& items) {
    const ImVec2 cell(92, 92);
    const float  gap    = 4.0f;
    int          perRow = std::max(1, (int)((ImGui::GetContentRegionAvail().x + gap) / (cell.x + gap)));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
    for (size_t i = 0; i < items.size(); ++i) {
        FsNode*     node = items[i];
        std::string path = VirtualFS::PathOf(node);
        if (i % perRow != 0)
            ImGui::SameLine();

        ImGui::PushID(path.c_str());
        bool doubleClicked = false;
        if (IconTile("##tile", IconForNode(node), node->name.c_str(), path == selected, cell, false, &doubleClicked))
            selected = path;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            selected = path;
        if (ImGui::BeginPopupContextItem("##item")) {
            ItemContextMenu(shell, node);
            ImGui::EndPopup();
        }
        if (doubleClicked) {
            selected = path;
            Open(shell, node);
        }
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
}

void FileExplorer::RenderPreview(Shell& shell) {
    FsNode* node = SelectedNode(shell);
    if (!node) {
        ImGui::TextDisabled("Select an item to see");
        ImGui::TextDisabled("its details.");
        return;
    }

    const float width = ImGui::GetContentRegionAvail().x;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    DrawIcon(ImGui::GetWindowDrawList(), IconForNode(node), ImVec2(pos.x + (width - 56.0f) * 0.5f, pos.y), 56.0f);
    ImGui::Dummy(ImVec2(width, 60.0f));

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(node->name.c_str());
    ImGui::PopTextWrapPos();
    ImGui::TextDisabled("%s", VirtualFS::TypeName(node).c_str());
    ImGui::Separator();

    if (node->isDir)
        ImGui::Text("Contains: %d items", (int)VirtualFS::CountItems(node));
    else
        ImGui::Text("Size: %s", VirtualFS::FormatSize(VirtualFS::SizeOf(node)).c_str());
    ImGui::Text("Modified: %s", VirtualFS::FormatTime(node->modified).c_str());
    ImGui::TextWrapped("Location: %s", node->parent ? VirtualFS::PathOf(node->parent).c_str() : "-");
    if (node->locked)
        ImGui::TextColored(WARN_COLOR, "Protected system item");
    else if (node->readOnly)
        ImGui::TextColored(WARN_COLOR, "Read-only");
    ImGui::Separator();

    switch (VirtualFS::KindOf(node)) {
    case FileKind::Image: {
        const Texture* tex = node->hostPath.empty() ? nullptr : &shell.textures.Get(node->hostPath);
        if (tex && tex->ok) {
            float h = width * (float)tex->height / (float)tex->width;
            ImGui::Image(tex->ImId(), ImVec2(width, h));
            if (ImGui::Button("Set as desktop background", ImVec2(-FLT_MIN, 0)))
                SetAsWallpaper(shell, node);
        } else {
            ImGui::TextDisabled("No preview available.");
        }
        break;
    }
    case FileKind::Text:
        if (ImGui::Button("Open in Notepad", ImVec2(-FLT_MIN, 0)))
            Open(shell, node);
        ImGui::BeginChild("##textpreview", ImVec2(0, 0), ImGuiChildFlags_Border);
        ImGui::PushFont(shell.fonts.mono);
        ImGui::PushTextWrapPos(0.0f);
        if (node->content.empty())
            ImGui::TextDisabled("(empty file)");
        else
            ImGui::TextUnformatted(node->content.c_str(),
                                   node->content.c_str() + std::min<size_t>(node->content.size(), 2000));
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        ImGui::EndChild();
        break;
    case FileKind::Folder:
        if (ImGui::Button("Open", ImVec2(-FLT_MIN, 0)))
            Open(shell, node);
        break;
    case FileKind::Program:
        if (ImGui::Button("Run", ImVec2(-FLT_MIN, 0)))
            Open(shell, node);
        break;
    case FileKind::Binary:
        ImGui::TextDisabled("Binary file - no preview.");
        break;
    }
}

void FileExplorer::RenderStatusBar(Shell& shell, size_t itemCount) {
    ImGui::Text("%d items", (int)itemCount);
    if (FsNode* sel = SelectedNode(shell)) {
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (sel->isDir)
            ImGui::Text("1 item selected");
        else
            ImGui::Text("1 item selected  %s", VirtualFS::FormatSize(VirtualFS::SizeOf(sel)).c_str());
    }
    if (!clipboardPath.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("|  %s: %s", clipboardCut ? "Cut" : "Copied",
                            VirtualFS::NameOf(clipboardPath).c_str());
    }
}

void FileExplorer::RenderDialogs(Shell& shell) {
    VirtualFS& fs = shell.fs;

    if (openRename) {
        ImGui::OpenPopup("Rename");
        openRename = false;
    }
    if (ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Rename '%s' to:", VirtualFS::NameOf(renameTarget).c_str());
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(320.0f);
        bool submit = ImGui::InputText("##newname", renameBuf, sizeof(renameBuf),
                                       ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (!renameError.empty())
            ImGui::TextColored(ERROR_COLOR, "%s", renameError.c_str());

        submit |= ImGui::Button("Rename", ImVec2(120, 0));
        ImGui::SameLine();
        bool cancel = ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape);

        if (submit) {
            FsNode* node = fs.Resolve(renameTarget);
            if (node && fs.Rename(node, renameBuf)) {
                if (clipboardPath == renameTarget)
                    clipboardPath = VirtualFS::PathOf(node);
                selected = VirtualFS::PathOf(node);
                ImGui::CloseCurrentPopup();
            } else {
                renameError = node ? fs.LastError() : "The item no longer exists.";
            }
        }
        if (cancel)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (openDelete) {
        ImGui::OpenPopup("Delete");
        openDelete = false;
    }
    if (ImGui::BeginPopupModal("Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        FsNode* node = fs.Resolve(deleteTarget);
        if (!node) {
            ImGui::CloseCurrentPopup();
        } else {
            if (node->isDir)
                ImGui::Text("Permanently delete the folder '%s' and everything in it?", node->name.c_str());
            else
                ImGui::Text("Permanently delete '%s'?", node->name.c_str());
            ImGui::TextDisabled("This can't be undone.");
            ImGui::Spacing();

            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                if (fs.Remove(node)) {
                    // A stale selection is cleared by Render() on the next frame.
                    if (clipboardPath == deleteTarget)
                        clipboardPath.clear();
                } else {
                    shell.Notify("Couldn't delete", fs.LastError(), ToastKind::Error);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            bool cancel = ImGui::Button("Cancel", ImVec2(120, 0));
            ImGui::SetItemDefaultFocus();   // the safe choice is the default
            if (cancel || ImGui::IsKeyPressed(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void FileExplorer::ItemContextMenu(Shell& shell, FsNode* node) {
    FileKind kind = VirtualFS::KindOf(node);

    if (ImGui::MenuItem(kind == FileKind::Program ? "Run" : "Open"))
        Open(shell, node);
    if (kind == FileKind::Image && !node->hostPath.empty() && ImGui::MenuItem("Set as desktop background"))
        SetAsWallpaper(shell, node);

    ImGui::Separator();
    if (ImGui::MenuItem("Cut", "Ctrl+X", false, !node->locked))
        SetClipboard(shell, node, true);
    if (ImGui::MenuItem("Copy", "Ctrl+C"))
        SetClipboard(shell, node, false);
    if (node->isDir && ImGui::MenuItem("Paste into folder", nullptr, false, !clipboardPath.empty()))
        Paste(shell, node);

    ImGui::Separator();
    if (ImGui::MenuItem("Rename", "F2", false, !node->locked))
        BeginRename(shell, node);
    if (ImGui::MenuItem("Delete", "Del", false, !node->locked))
        BeginDelete(shell, node);

    ImGui::Separator();
    if (ImGui::MenuItem("Copy path"))
        ImGui::SetClipboardText(VirtualFS::PathOf(node).c_str());
}

void FileExplorer::HandleShortcuts(Shell& shell, FsNode* dir) {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::GetIO().WantTextInput)
        return;

    FsNode* sel = SelectedNode(shell);
    if (ImGui::GetIO().KeyCtrl) {
        if (sel && ImGui::IsKeyPressed(ImGuiKey_C, false)) SetClipboard(shell, sel, false);
        if (sel && ImGui::IsKeyPressed(ImGuiKey_X, false)) SetClipboard(shell, sel, true);
        if (!clipboardPath.empty() && ImGui::IsKeyPressed(ImGuiKey_V, false)) Paste(shell, dir);
        return;
    }
    if (sel && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) BeginDelete(shell, sel);
    if (sel && ImGui::IsKeyPressed(ImGuiKey_F2, false))     BeginRename(shell, sel);
    if (sel && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
        Open(shell, sel);
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
        GoBack();
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void FileExplorer::Open(Shell& shell, FsNode* node) {
    if (node->isDir)
        Navigate(VirtualFS::PathOf(node));
    else
        shell.OpenPath(VirtualFS::PathOf(node));
}

void FileExplorer::CreateItem(Shell& shell, FsNode* dir, bool folder) {
    VirtualFS&  fs   = shell.fs;
    std::string name = fs.UniqueName(dir, folder ? "New folder" : "New Text Document.txt");
    FsNode*     node = folder ? fs.NewFolder(dir, name) : fs.NewFile(dir, name);
    if (!node) {
        shell.Notify("Can't create item", fs.LastError(), ToastKind::Error);
        return;
    }
    searchBuf[0] = '\0';
    selected     = VirtualFS::PathOf(node);
    BeginRename(shell, node);
}

void FileExplorer::BeginRename(Shell& shell, FsNode* node) {
    if (node->locked) {
        shell.Notify("Rename", "'" + node->name + "' is a protected system item.", ToastKind::Warning);
        return;
    }
    renameTarget = VirtualFS::PathOf(node);
    snprintf(renameBuf, sizeof(renameBuf), "%s", node->name.c_str());
    renameError.clear();
    openRename = true;
}

void FileExplorer::BeginDelete(Shell& shell, FsNode* node) {
    if (node->locked) {
        shell.Notify("Delete", "'" + node->name + "' is a protected system item.", ToastKind::Warning);
        return;
    }
    deleteTarget = VirtualFS::PathOf(node);
    openDelete   = true;
}

void FileExplorer::SetClipboard(Shell& shell, FsNode* node, bool cut) {
    if (cut && node->locked) {
        shell.Notify("Cut", "'" + node->name + "' is a protected system item.", ToastKind::Warning);
        return;
    }
    clipboardPath = VirtualFS::PathOf(node);
    clipboardCut  = cut;
}

void FileExplorer::Paste(Shell& shell, FsNode* dest) {
    VirtualFS& fs  = shell.fs;
    FsNode*    src = fs.Resolve(clipboardPath);
    if (!src) {
        shell.Notify("Paste", "The item on the clipboard no longer exists.", ToastKind::Warning);
        clipboardPath.clear();
        return;
    }

    FsNode* result = clipboardCut ? fs.Move(src, dest) : fs.Copy(src, dest);
    if (!result) {
        shell.Notify("Paste failed", fs.LastError(), ToastKind::Error);
        return;
    }
    if (result->parent && VirtualFS::PathOf(result->parent) == cwd)
        selected = VirtualFS::PathOf(result);
    if (clipboardCut)
        clipboardPath.clear();
}

void FileExplorer::SetAsWallpaper(Shell& shell, FsNode* image) {
    if (shell.SetWallpaper(image->hostPath))
        shell.Notify("Personalization", "Desktop background changed to " + image->name + ".", ToastKind::Success);
}
