// PrismaUI_F4/src/Menus/FocusMenu/FocusMenu.h
#pragma once

#include <RE/Fallout.h>

class FocusMenu : public RE::IMenu
{
public:
    static constexpr std::string_view MENU_NAME = "PrismaUI_FocusMenu";

    // F4 signature: second param is uint64_t (not uint32_t as in Skyrim)
    void AdvanceMovie(float a_interval, std::uint64_t a_currentTime) override;
    RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override;

    static RE::IMenu* Creator(const RE::UIMessage& a_message);

    static void Open();
    static void Close();
    static bool IsOpen();

    bool IsValid() const { return uiMovie != nullptr; }

private:
    FocusMenu();
};
