#pragma once
#include "focus_rules.h"
#include "power.h"
#include <windows.h>

// Modal dialog used to view/edit auto-switch rules and the fallback plan.
// Loads the current ruleset from the registry on open and saves it when the
// user clicks Save. Returns true when the ruleset was saved.
namespace focus_ui {
// Initialize global UI resources before creating dialog windows.
void init(HINSTANCE instance, HFONT font);
bool show_rules(HWND owner, const std::vector<power::Item>& schemes);

// A focus-rule/picker dialog is modal (it owns a private message loop). While
// one is open the tray menu must not be shown; instead the tray click returns
// focus to that dialog. This lets main_proc know the currently open dialog.
HWND current_modal();
}

