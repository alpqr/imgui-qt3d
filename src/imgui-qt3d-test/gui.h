#ifndef GUI_H
#define GUI_H

#include <imgui.h>

class ImguiManager;

class Gui
{
public:
    void setManager(ImguiManager *mgr) { m_manager = mgr; }
    void frame();

private:
    ImguiManager *m_manager = nullptr;
    bool show_test_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
};


#endif
