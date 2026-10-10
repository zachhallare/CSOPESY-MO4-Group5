// Notepad: tabbed plain-text editor for files in the virtual file system.
#pragma once

#include <string>
#include <vector>
#include "apps/app.h"

class Notepad : public App {
public:
    Notepad() : App(AppID::Notepad) {}

    std::string      Title() const override;
    ImVec2           DefaultSize() const override { return ImVec2(700, 460); }
    ImGuiWindowFlags WindowFlags() const override { return ImGuiWindowFlags_MenuBar; }
    void             OnOpen(Shell& shell) override;
    void             Render(Shell& shell) override;
    bool             OnCloseRequest(Shell& shell) override;
    void             OnClose(Shell& shell) override;

    void OpenFile(Shell& shell, const std::string& path);
    bool HasUnsavedChanges() const;

private:
    struct Document {
        int         id = 0;
        std::string path;        // empty until first saved
        std::string name;        // tab label
        std::string text;
        std::string savedText;
        bool        readOnly = false;

        bool Dirty() const { return text != savedText; }
    };

    std::vector<Document> docs;
    int  activeId        = -1;   // id of the selected tab
    int  selectRequest   = -1;   // id of a tab to select on the next frame
    int  nextId          = 1;
    int  untitledCount   = 0;
    int  cursorLine      = 1;
    int  cursorCol       = 1;
    bool insertTimestamp = false;

    // Dialog state
    bool        openSaveAs       = false;
    int         saveAsId         = -1;
    bool        closeAfterSaveAs = false;
    char        pathBuf[256]     = "";
    std::string dialogError;
    bool        openFileDialog   = false;
    char        filterBuf[64]    = "";
    std::string openSelection;
    bool        openCloseConfirm = false;
    int         closeId          = -1;
    bool        openExitConfirm  = false;

    Document* Find(int id);
    Document* Active();
    void NewDocument();
    void CloseDocument(int id);
    void RequestCloseDocument(int id);
    bool Save(Shell& shell, Document& doc);   // false when it still needs a path (Save As opens)
    bool SaveTo(Shell& shell, Document& doc, const std::string& path, std::string* error);
    bool SaveOrAutoName(Shell& shell, Document& doc);
    void BeginSaveAs(const Document& doc, bool closeAfter);

    void RenderMenuBar(Shell& shell);
    void RenderStatusBar();
    void RenderDialogs(Shell& shell);
    static int TextCallback(ImGuiInputTextCallbackData* data);
};
