/**
 * @file menu_system.cpp
 * @brief Menu System Implementation for PXLcam v1.2.0
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#include "menu_system.h"
#include <Arduino.h>

namespace pxlcam {
namespace features {

MenuSystem::MenuSystem()
    : selectedIndex_(0)
    , isOpen_(false)
{
}

void MenuSystem::init() {
    Serial.println("[MenuSystem] Initializing...");
    
    menus_.clear();
    menuStack_.clear();
    selectedIndex_ = 0;
    isOpen_ = false;

    createDefaultMenus();
    
    Serial.printf("[MenuSystem] Initialized with %zu menus\n", menus_.size());
}

void MenuSystem::createDefaultMenus() {
    // Main Menu (ID: 0) - 3 required items per user spec
    MenuDef mainMenu;
    mainMenu.id = MAIN_MENU_ID;
    mainMenu.title = "PXLcam";
    mainMenu.items = {
        MenuItem::createSubmenu(
            "Style Mode",
            "Select capture style",
            STYLE_MODE_MENU_ID
        ),
        MenuItem::createSubmenu(
            "Settings",
            "Camera settings",
            CAPTURE_SETTINGS_ID
        ),
        MenuItem::createSubmenu(
            "About",
            "Device info",
            ABOUT_MENU_ID
        )
    };
    addMenu(mainMenu);

    // Style Mode Submenu (ID: 4) - New per user spec
    MenuDef styleMenu;
    styleMenu.id = STYLE_MODE_MENU_ID;
    styleMenu.title = "Style Mode";
    styleMenu.items = {
        MenuItem::createAction(
            "Normal",
            "Standard capture",
            []() { Serial.println("[Menu] Style: Normal"); }
        ),
        MenuItem::createAction(
            "GameBoy",
            "4-tone dithering",
            []() { Serial.println("[Menu] Style: GameBoy"); }
        ),
        MenuItem::createAction(
            "Night",
            "Gamma boost mode",
            []() { Serial.println("[Menu] Style: Night"); }
        ),
        MenuItem::createBack()
    };
    addMenu(styleMenu);

    // Settings Submenu (ID: 1)
    MenuDef settingsMenu;
    settingsMenu.id = CAPTURE_SETTINGS_ID;
    settingsMenu.title = "Settings";
    settingsMenu.items = {
        MenuItem::createAction(
            "Auto Exposure",
            "Toggle AE",
            []() { Serial.println("[Menu] Toggle AE"); }
        ),
        MenuItem::createAction(
            "Night Mode",
            "Toggle night boost",
            []() { Serial.println("[Menu] Toggle Night"); }
        ),
        MenuItem::createAction(
            "Reset",
            "Factory defaults",
            []() { Serial.println("[Menu] Reset settings"); }
        ),
        MenuItem::createBack()
    };
    addMenu(settingsMenu);

    // About Submenu (ID: 3)
    MenuDef aboutMenu;
    aboutMenu.id = ABOUT_MENU_ID;
    aboutMenu.title = "About PXLcam";
    aboutMenu.items = {
        MenuItem::createAction(
            "Version: 1.2.0",
            "Firmware version",
            []() { Serial.println("[Menu] Version info"); }
        ),
        MenuItem::createAction(
            "ESP32-CAM",
            "Hardware platform",
            []() { Serial.println("[Menu] Hardware info"); }
        ),
        MenuItem::createAction(
            "License: MIT",
            "Open source license",
            []() { Serial.println("[Menu] License info"); }
        ),
        MenuItem::createAction(
            "Free Memory",
            "Show available RAM",
            []() { 
                Serial.printf("[Menu] Free heap: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
            }
        ),
        MenuItem::createBack()
    };
    addMenu(aboutMenu);
}

void MenuSystem::addMenu(const MenuDef& menu) {
    // Check if menu with this ID already exists
    for (auto& existing : menus_) {
        if (existing.id == menu.id) {
            existing = menu;
            Serial.printf("[MenuSystem] Updated menu ID %d: '%s'\n", menu.id, menu.title);
            return;
        }
    }
    
    menus_.push_back(menu);
    Serial.printf("[MenuSystem] Added menu ID %d: '%s' (%zu items)\n", 
                  menu.id, menu.title, menu.items.size());
}

bool MenuSystem::open(int menuId) {
    MenuDef* menu = findMenu(menuId);
    if (!menu) {
        Serial.printf("[MenuSystem] Menu ID %d not found\n", menuId);
        return false;
    }

    menuStack_.clear();
    menuStack_.push_back(menuId);
    selectedIndex_ = 0;
    isOpen_ = true;

    Serial.printf("[MenuSystem] Opened menu: '%s'\n", menu->title);
    
    if (onMenuChange_) {
        onMenuChange_(true);
    }

    return true;
}

void MenuSystem::close() {
    if (!isOpen_) return;

    menuStack_.clear();
    selectedIndex_ = 0;
    isOpen_ = false;

    Serial.println("[MenuSystem] Closed");
    
    if (onMenuChange_) {
        onMenuChange_(false);
    }
}

void MenuSystem::navigateUp() {
    if (!isOpen_ || menuStack_.empty()) return;

    MenuDef* menu = findMenu(menuStack_.back());
    if (!menu || menu->items.empty()) return;

    int prevIndex = selectedIndex_;
    
    if (selectedIndex_ > 0) {
        selectedIndex_--;
    } else {
        selectedIndex_ = menu->items.size() - 1;  // Wrap to bottom
    }

    // Skip disabled items
    int attempts = 0;
    while (!menu->items[selectedIndex_].enabled && attempts < (int)menu->items.size()) {
        selectedIndex_ = (selectedIndex_ - 1 + menu->items.size()) % menu->items.size();
        attempts++;
    }

    if (selectedIndex_ != prevIndex && onSelectionChange_) {
        onSelectionChange_(selectedIndex_);
    }

    Serial.printf("[MenuSystem] Navigate up: index %d\n", selectedIndex_);
}

void MenuSystem::navigateDown() {
    if (!isOpen_ || menuStack_.empty()) return;

    MenuDef* menu = findMenu(menuStack_.back());
    if (!menu || menu->items.empty()) return;

    int prevIndex = selectedIndex_;

    if (selectedIndex_ < (int)menu->items.size() - 1) {
        selectedIndex_++;
    } else {
        selectedIndex_ = 0;  // Wrap to top
    }

    // Skip disabled items
    int attempts = 0;
    while (!menu->items[selectedIndex_].enabled && attempts < (int)menu->items.size()) {
        selectedIndex_ = (selectedIndex_ + 1) % menu->items.size();
        attempts++;
    }

    if (selectedIndex_ != prevIndex && onSelectionChange_) {
        onSelectionChange_(selectedIndex_);
    }

    Serial.printf("[MenuSystem] Navigate down: index %d\n", selectedIndex_);
}

void MenuSystem::navigateNext() {
    // Alias for navigateDown - used for single-button navigation
    // Short press cycles to next item (wraps around)
    navigateDown();
    Serial.printf("[MenuSystem] Navigate next (single-button mode)\n");
}

MenuResult MenuSystem::select() {
    if (!isOpen_ || menuStack_.empty()) return MenuResult::NONE;

    MenuDef* menu = findMenu(menuStack_.back());
    if (!menu || selectedIndex_ >= (int)menu->items.size()) return MenuResult::NONE;

    MenuItem& item = menu->items[selectedIndex_];
    if (!item.enabled) return MenuResult::NONE;

    Serial.printf("[MenuSystem] Selected: '%s'\n", item.label);

    switch (item.type) {
        case MenuItemType::ACTION:
            if (item.action) {
                item.action();
            }
            return MenuResult::SELECTED;

        case MenuItemType::SUBMENU:
            if (item.submenuId >= 0 && findMenu(item.submenuId)) {
                menuStack_.push_back(item.submenuId);
                selectedIndex_ = 0;
                if (onMenuChange_) {
                    onMenuChange_(true);
                }
                return MenuResult::SELECTED;
            }
            break;

        case MenuItemType::BACK:
            return back();

        case MenuItemType::TOGGLE:
            // TODO: Implement toggle logic
            return MenuResult::SELECTED;

        case MenuItemType::VALUE:
            // TODO: Implement value adjustment
            return MenuResult::SELECTED;

        default:
            break;
    }

    return MenuResult::NONE;
}

MenuResult MenuSystem::back() {
    if (!isOpen_ || menuStack_.empty()) return MenuResult::NONE;

    if (menuStack_.size() > 1) {
        menuStack_.pop_back();
        selectedIndex_ = 0;
        Serial.printf("[MenuSystem] Back to menu ID %d\n", menuStack_.back());
        return MenuResult::BACK;
    }

    // At root menu - exit
    close();
    return MenuResult::EXIT;
}

int MenuSystem::getCurrentMenuId() const {
    if (!isOpen_ || menuStack_.empty()) return -1;
    return menuStack_.back();
}

const char* MenuSystem::getCurrentMenuTitle() const {
    if (!isOpen_ || menuStack_.empty()) return "";
    
    for (const auto& menu : menus_) {
        if (menu.id == menuStack_.back()) {
            return menu.title;
        }
    }
    return "";
}

const std::vector<MenuItem>* MenuSystem::getCurrentItems() const {
    if (!isOpen_ || menuStack_.empty()) return nullptr;
    
    for (const auto& menu : menus_) {
        if (menu.id == menuStack_.back()) {
            return &menu.items;
        }
    }
    return nullptr;
}

const MenuItem* MenuSystem::getSelectedItem() const {
    const auto* items = getCurrentItems();
    if (!items || selectedIndex_ >= (int)items->size()) return nullptr;
    return &(*items)[selectedIndex_];
}

size_t MenuSystem::getItemCount() const {
    const auto* items = getCurrentItems();
    return items ? items->size() : 0;
}

MenuDef* MenuSystem::findMenu(int menuId) {
    for (auto& menu : menus_) {
        if (menu.id == menuId) {
            return &menu;
        }
    }
    return nullptr;
}

} // namespace features
} // namespace pxlcam
