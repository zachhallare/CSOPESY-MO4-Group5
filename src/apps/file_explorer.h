// File Explorer: browses and manages the in-memory file system. Navigate,
// search, preview, and create / rename / cut / copy / paste / delete items.
#pragma once

#include <string>
#include <vector>
#include "apps/app.h"

struct FsNode;

class FileExplorer : public App {
public:
    FileExplorer() : App(AppID::FileExplorer) {}

    std::string Title() const override;
    ImVec2      DefaultSize() const override { return ImVec2(920, 540); }
    ImVec2      MinSize() const override     { return ImVec2(620, 340); }
    void        Render(Shell& shell) override;

    void Navigate(const std::string& folderPath);   // records history
    void Reveal(const std::string& filePath);       // opens the parent folder with the file selected

private:
    std::string              cwd = "/";
    std::vector<std::string> backStack, forwardStack;
    std::string              selected;              // full path of the selected item
    char                     searchBuf[128] = "";
    bool                     iconView = false;
    bool                     syncTree = true;       // expand the tree down to cwd

    std::string clipboardPath;
    bool        clipboardCut = false;

    std::string renameTarget;
    char        renameBuf[128] = "";
    std::string renameError;
    bool        openRename = false;

    std::string deleteTarget;
    bool        openDelete = false;

    FsNode* SelectedNode(Shell& shell) const;

    void GoBack();
    void GoForward();
    void GoUp();

    void RenderToolbar();
    void RenderCommandBar(Shell& shell, FsNode* dir);
    void RenderNavPane(Shell& shell);
    void RenderNavTree(FsNode* node);
    void RenderContents(Shell& shell, FsNode* dir, size_t* itemCount);
    void RenderDetails(Shell& shell, std::vector<FsNode*>& items, bool searching);
    void RenderIcons(Shell& shell, const std::vector<FsNode*>& items);
    void RenderPreview(Shell& shell);
    void RenderStatusBar(Shell& shell, size_t itemCount);
    void RenderDialogs(Shell& shell);
    void ItemContextMenu(Shell& shell, FsNode* node);
    void HandleShortcuts(Shell& shell, FsNode* dir);

    void Open(Shell& shell, FsNode* node);
    void CreateItem(Shell& shell, FsNode* dir, bool folder);
    void BeginRename(Shell& shell, FsNode* node);
    void BeginDelete(Shell& shell, FsNode* node);
    void SetClipboard(Shell& shell, FsNode* node, bool cut);
    void Paste(Shell& shell, FsNode* dest);
    void SetAsWallpaper(Shell& shell, FsNode* image);
};
