/**
 * @file menu_system.h
 * @brief Menu System for PXLcam v1.2.0
 * 
 * Provides a flexible menu system with:
 * - Hierarchical menu structure
 * - Navigation (up/down/select/back)
 * - Customizable menu items
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#ifndef PXLCAM_MENU_SYSTEM_H
#define PXLCAM_MENU_SYSTEM_H

#include <cstdint>
#include <functional>
#include <vector>

namespace pxlcam {
namespace features {

/**
 * @brief Menu item types
 */
enum class MenuItemType : uint8_t {
    ACTION = 0,     ///< Executes an action when selected
    SUBMENU,        ///< Opens a submenu
    TOGGLE,         ///< Boolean toggle
    VALUE,          ///< Numeric value adjustment
    BACK            ///< Return to parent menu
};

/**
 * @brief Menu item action callback
 */
using MenuAction = std::function<void()>;

/**
 * @brief Menu item structure
 */
struct MenuItem {
    const char* label;          ///< Display label
    const char* description;    ///< Optional description/hint
    MenuItemType type;          ///< Item type
    MenuAction action;          ///< Action callback (for ACTION type)
    int submenuId;              ///< Submenu ID (for SUBMENU type)
    bool enabled;               ///< Item enabled state

    /**
     * @brief Create an action menu item
     */
    static MenuItem createAction(const char* label, const char* desc, MenuAction action) {
        return { label, desc, MenuItemType::ACTION, action, -1, true };
    }

    /**
     * @brief Create a submenu item
     */
    static MenuItem createSubmenu(const char* label, const char* desc, int submenuId) {
        return { label, desc, MenuItemType::SUBMENU, nullptr, submenuId, true };
    }

    /**
     * @brief Create a back item
     */
    static MenuItem createBack(const char* label = "< Back") {
        return { label, "Return to previous menu", MenuItemType::BACK, nullptr, -1, true };
    }
};

/**
 * @brief Menu definition structure
 */
struct MenuDef {
    int id;                         ///< Menu identifier
    const char* title;              ///< Menu title
    std::vector<MenuItem> items;    ///< Menu items
};

/**
 * @brief Menu navigation result
 */
enum class MenuResult : uint8_t {
    NONE = 0,       ///< No action taken
    SELECTED,       ///< Item selected
    BACK,           ///< Back navigation
    EXIT            ///< Exit menu system
};

/**
 * @brief Menu System class
 * 
 * Manages menu navigation and rendering.
 * 
 * @code
 * MenuSystem menu;
 * menu.init();
 * menu.addMenu({0, "Main Menu", {
 *     MenuItem::createAction("Preview", "Start preview", []() { ... }),
 *     MenuItem::createSubmenu("Settings", "Configure camera", 1),
 *     MenuItem::createAction("About", "Device info", []() { ... })
 * }});
 * menu.open(0);  // Open main menu
 * @endcode
 */
class MenuSystem {
public:
    /// Main menu ID constant
    static constexpr int MAIN_MENU_ID = 0;
    
    /// Capture settings submenu ID
    static constexpr int CAPTURE_SETTINGS_ID = 1;
    
    /// Display settings submenu ID
    static constexpr int DISPLAY_SETTINGS_ID = 2;
    
    /// About submenu ID
    static constexpr int ABOUT_MENU_ID = 3;

    /**
     * @brief Constructor
     */
    MenuSystem();

    /**
     * @brief Destructor
     */
    ~MenuSystem() = default;

    /**
     * @brief Initialize menu system with default menus
     */
    void init();

    /**
     * @brief Add a menu definition
     * @param menu Menu definition to add
     */
    void addMenu(const MenuDef& menu);

    /**
     * @brief Open a menu by ID
     * @param menuId Menu identifier
     * @return bool True if menu exists
     */
    bool open(int menuId);

    /**
     * @brief Close menu system
     */
    void close();

    /**
     * @brief Navigate to next item (wraps around)
     * For single-button navigation: short press = next
     */
    void navigateNext();

    /**
     * @brief Navigate up in current menu
     */
    void navigateUp();

    /**
     * @brief Navigate down in current menu
     */
    void navigateDown();

    /**
     * @brief Select current item
     * @return MenuResult Result of selection
     */
    MenuResult select();

    /**
     * @brief Go back to parent menu
     * @return MenuResult Result of back action
     */
    MenuResult back();

    /**
     * @brief Check if menu is open
     * @return bool True if open
     */
    bool isOpen() const { return isOpen_; }

    /**
     * @brief Get current menu ID
     * @return int Menu ID or -1 if not open
     */
    int getCurrentMenuId() const;

    /**
     * @brief Get current menu title
     * @return const char* Menu title
     */
    const char* getCurrentMenuTitle() const;

    /**
     * @brief Get selected item index
     * @return int Item index
     */
    int getSelectedIndex() const { return selectedIndex_; }

    /**
     * @brief Get current menu items
     * @return const std::vector<MenuItem>* Items or nullptr
     */
    const std::vector<MenuItem>* getCurrentItems() const;

    /**
     * @brief Get selected item
     * @return const MenuItem* Selected item or nullptr
     */
    const MenuItem* getSelectedItem() const;

    /**
     * @brief Get menu item count for current menu
     * @return size_t Item count
     */
    size_t getItemCount() const;

    /**
     * @brief Set callback for when selection changes
     * @param callback Callback function
     */
    void setOnSelectionChange(std::function<void(int)> callback) {
        onSelectionChange_ = callback;
    }

    /**
     * @brief Set callback for when menu opens/closes
     * @param callback Callback function (bool = isOpen)
     */
    void setOnMenuChange(std::function<void(bool)> callback) {
        onMenuChange_ = callback;
    }

private:
    /**
     * @brief Create default menus (Main, Settings, About)
     */
    void createDefaultMenus();

    /**
     * @brief Find menu by ID
     * @param menuId Menu ID
     * @return MenuDef* Menu or nullptr
     */
    MenuDef* findMenu(int menuId);

    std::vector<MenuDef> menus_;            ///< All menu definitions
    std::vector<int> menuStack_;            ///< Menu navigation stack
    int selectedIndex_;                     ///< Currently selected item
    bool isOpen_;                           ///< Menu open state

    std::function<void(int)> onSelectionChange_;
    std::function<void(bool)> onMenuChange_;
};

} // namespace features
} // namespace pxlcam

#endif // PXLCAM_MENU_SYSTEM_H
