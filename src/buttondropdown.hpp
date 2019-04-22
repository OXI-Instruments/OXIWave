#pragma once

/*
Thanks to @ghost for the basis for this code (modified by Dan Green)
https://github.com/ocornut/imgui/issues/474
*/

namespace ImGui {

IMGUI_API bool BeginButtonDropDown(const char* label, ImVec2 buttonSize)
{
    ImGui::SameLine(0.f, 0.f);

    ImGuiWindow* window = GetCurrentWindow();
    ImGuiStyle& style = ImGui::GetStyle();

    float x = ImGui::GetCursorPosX();
    float y = ImGui::GetCursorPosY();

    ImVec2 size(buttonSize.x + 20, buttonSize.y);

    ImGui::Dummy(ImVec2(00.0f,0.0f));   
    ImGui::SameLine();
    bool pressed = ImGui::Button(label, size);

    // Arrow
    ImVec2 center(window->Pos.x + buttonSize.x + x + 20, window->Pos.y + y + buttonSize.y / 2);
    float r = 8.f;
    center.y -= r * 0.25f;
    ImVec2 a = center + ImVec2(0, 1) * r;
    ImVec2 b = center + ImVec2(-0.866f, -0.5f) * r;
    ImVec2 c = center + ImVec2(0.866f, -0.5f) * r;

    window->DrawList->AddTriangleFilled(a, b, c, GetColorU32(ImGuiCol_Text));

    // Popup

    ImVec2 popupPos;

    popupPos.x = window->Pos.x + x + buttonSize.x/2;
    popupPos.y = window->Pos.y + y + buttonSize.y;

    ImGui::SetNextWindowPos(popupPos);

    if (pressed)
    {
        ImGui::OpenPopup(label);
    }

    if (ImGui::BeginPopup(label))
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, style.Colors[ImGuiCol_Button]);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, style.Colors[ImGuiCol_Button]);
        ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, style.Colors[ImGuiCol_Button]);
        return true;
    }

    return false;
}

void EndButtonDropDown()
{
    ImGui::PopStyleColor(3);
    ImGui::EndPopup();
}

} // namespace ImGui
