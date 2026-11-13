// [XCDE] T261: shared native UI theme; no textures, web runtime or decorative animation.
#pragma once
#include "imgui.h"
static inline ImVec4 monado_rgb(unsigned rgb) {
    return ImVec4(((rgb>>16)&255)/255.f,((rgb>>8)&255)/255.f,(rgb&255)/255.f,1);
}
static inline void monado_theme(float scale) {
    ImGui::StyleColorsDark();
    ImGuiStyle &s=ImGui::GetStyle();
    s.WindowRounding=12; s.ChildRounding=12; s.FrameRounding=8;
    s.PopupRounding=8; s.ScrollbarRounding=8; s.GrabRounding=6; s.TabRounding=7;
    s.WindowPadding=ImVec2(18,16); s.FramePadding=ImVec2(10,7);
    s.ItemSpacing=ImVec2(12,10); s.ItemInnerSpacing=ImVec2(8,6);
    s.WindowBorderSize=1; s.ChildBorderSize=1; s.FrameBorderSize=1;
    auto &c=s.Colors;
    c[ImGuiCol_Text]=monado_rgb(0xEDF2EE); c[ImGuiCol_TextDisabled]=monado_rgb(0xB3BFB6);
    c[ImGuiCol_WindowBg]=monado_rgb(0x101411); c[ImGuiCol_ChildBg]=monado_rgb(0x19211C);
    c[ImGuiCol_PopupBg]=monado_rgb(0x19211C); c[ImGuiCol_Border]=monado_rgb(0x304438);
    c[ImGuiCol_TitleBg]=monado_rgb(0x101411); c[ImGuiCol_TitleBgActive]=monado_rgb(0x203D2A);
    c[ImGuiCol_TitleBgCollapsed]=monado_rgb(0x19211C); c[ImGuiCol_MenuBarBg]=monado_rgb(0x19211C);
    c[ImGuiCol_FrameBg]=monado_rgb(0x202C24); c[ImGuiCol_FrameBgHovered]=monado_rgb(0x2B4033);
    c[ImGuiCol_FrameBgActive]=monado_rgb(0x35533F);
    c[ImGuiCol_Button]=monado_rgb(0x294A35); c[ImGuiCol_ButtonHovered]=monado_rgb(0x386347);
    c[ImGuiCol_ButtonActive]=monado_rgb(0x437A55);
    c[ImGuiCol_CheckMark]=c[ImGuiCol_SliderGrab]=monado_rgb(0x59C985);
    c[ImGuiCol_SliderGrabActive]=monado_rgb(0x86DDA8);
    c[ImGuiCol_Header]=monado_rgb(0x294A35); c[ImGuiCol_HeaderHovered]=monado_rgb(0x386347);
    c[ImGuiCol_HeaderActive]=monado_rgb(0x437A55);
    c[ImGuiCol_Tab]=monado_rgb(0x19211C); c[ImGuiCol_TabHovered]=monado_rgb(0x386347);
    c[ImGuiCol_TabSelected]=monado_rgb(0x294A35); c[ImGuiCol_Separator]=monado_rgb(0x304438);
    c[ImGuiCol_TabSelectedOverline]=monado_rgb(0x59C985);
    c[ImGuiCol_TextSelectedBg]=ImVec4(0.18f,0.35f,0.24f,0.85f);
    c[ImGuiCol_ResizeGrip]=monado_rgb(0x294A35); c[ImGuiCol_ResizeGripHovered]=monado_rgb(0x386347);
    c[ImGuiCol_ResizeGripActive]=monado_rgb(0x59C985);
    c[ImGuiCol_NavCursor]=monado_rgb(0x86DDA8); c[ImGuiCol_PlotHistogram]=monado_rgb(0x59C985);
    s.ScaleAllSizes(scale>0 ? scale : 1);
}
// [XCDE] Launcher theme: deep navy panels, one crimson accent (selected page, PLAY) and teal for checkmarks,
// sliders and small labels. The F1 menu keeps monado_theme().
static inline ImVec4 monado_teal() { return monado_rgb(0x5FD4DE); }
static inline ImVec4 monado_crimson() { return monado_rgb(0xBA213B); }
static inline void monado_launcher_theme(float scale) {
    ImGui::StyleColorsDark();
    ImGuiStyle &s=ImGui::GetStyle();
    s.WindowPadding=ImVec2(22,18); s.FramePadding=ImVec2(12,7);
    s.ItemSpacing=ImVec2(12,10); s.ItemInnerSpacing=ImVec2(8,6);
    s.WindowRounding=0; s.ChildRounding=10; s.FrameRounding=6;
    s.PopupRounding=8; s.ScrollbarRounding=8; s.GrabRounding=4; s.TabRounding=6;
    s.WindowBorderSize=0; s.ChildBorderSize=0; s.FrameBorderSize=0;
    auto &c=s.Colors;
    c[ImGuiCol_WindowBg]=ImVec4(.035f,.05f,.07f,1); c[ImGuiCol_ChildBg]=ImVec4(.06f,.08f,.105f,1);
    c[ImGuiCol_PopupBg]=ImVec4(.075f,.095f,.12f,1);
    c[ImGuiCol_Text]=ImVec4(.91f,.94f,.96f,1); c[ImGuiCol_TextDisabled]=ImVec4(.52f,.62f,.69f,1);
    c[ImGuiCol_Border]=ImVec4(.18f,.24f,.29f,.6f);
    c[ImGuiCol_FrameBg]=ImVec4(.10f,.14f,.18f,1); c[ImGuiCol_FrameBgHovered]=ImVec4(.15f,.22f,.27f,1);
    c[ImGuiCol_FrameBgActive]=ImVec4(.19f,.28f,.33f,1);
    c[ImGuiCol_Button]=ImVec4(.13f,.19f,.23f,1); c[ImGuiCol_ButtonHovered]=ImVec4(.20f,.29f,.34f,1);
    c[ImGuiCol_ButtonActive]=ImVec4(.26f,.36f,.40f,1);
    c[ImGuiCol_CheckMark]=c[ImGuiCol_SliderGrab]=ImVec4(.31f,.83f,.89f,1);
    c[ImGuiCol_SliderGrabActive]=ImVec4(.63f,.96f,1,1);
    c[ImGuiCol_Header]=ImVec4(.13f,.19f,.23f,1); c[ImGuiCol_HeaderHovered]=ImVec4(.20f,.29f,.34f,1);
    c[ImGuiCol_HeaderActive]=ImVec4(.26f,.36f,.40f,1);
    c[ImGuiCol_TabSelected]=ImVec4(.50f,.13f,.19f,1); c[ImGuiCol_TabHovered]=ImVec4(.65f,.18f,.24f,1);
    c[ImGuiCol_Tab]=ImVec4(.10f,.15f,.19f,1); c[ImGuiCol_TabSelectedOverline]=ImVec4(.31f,.83f,.89f,1);
    c[ImGuiCol_Separator]=ImVec4(.17f,.25f,.29f,1);
    c[ImGuiCol_TitleBg]=c[ImGuiCol_TitleBgActive]=ImVec4(.13f,.08f,.11f,1);
    c[ImGuiCol_TextSelectedBg]=ImVec4(.31f,.83f,.89f,.35f);
    c[ImGuiCol_NavCursor]=ImVec4(.63f,.96f,1,1);
    c[ImGuiCol_PlotHistogram]=ImVec4(.28f,.78f,.84f,1);
    s.ScaleAllSizes(scale>0 ? scale : 1);
}
