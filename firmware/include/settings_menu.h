/**
 * @file settings_menu.h
 * @brief On-device settings menu for OpenClaw Cardputer
 *
 * Features:
 * - Navigate settings with keyboard (arrow keys, Enter, Backspace)
 * - Edit WiFi, Gateway, Device, and Audio settings
 * - Save to SPIFFS /config.json
 * - Live validation and testing
 */

#ifndef OPENCLAW_SETTINGS_MENU_H
#define OPENCLAW_SETTINGS_MENU_H

#include <Arduino.h>
#include <functional>
#include "config_manager.h"
#include "display_renderer.h"
#include "keyboard_handler.h"

namespace OpenClaw {

// Forward declaration
class SettingsMenu;

// Menu states
enum class MenuState {
    CLOSED,           // Menu not active
    MAIN_MENU,        // Top-level category selection
    EDIT_ITEM,        // Editing a specific setting
    CONFIRM_SAVE,     // Confirm save dialog
    TEST_CONNECTION,  // Testing WiFi/Gateway
    SHOW_MESSAGE      // Showing info/error message
};

// Menu categories
enum class MenuCategory {
    WIFI,
    GATEWAY,
    DEVICE,
    AUDIO,
    SYSTEM,
    TOOLS,
    COUNT
};

// Menu item types
enum class MenuItemType {
    STRING,           // Text input (SSID, passwords, URLs)
    INTEGER,          // Numeric value (port, gain, brightness)
    BOOLEAN,          // Toggle (dhcp, auto_connect, etc.)
    ENUM,             // Selection from list (codec)
    ACTION,           // Perform action (save, test, reset)
    SUBMENU           // Navigate to sub-menu
};

// Menu item structure
struct MenuItem {
    const char* label;
    MenuItemType type;
    MenuCategory category;
    void* value_ptr;           // Pointer to config value
    size_t max_length;         // For strings
    int min_value;             // For integers
    int max_value;             // For integers
    const char** enum_options; // For enums (null-terminated array)
    void (*action)();          // For actions
    const char* help_text;     // Tooltip/help
};

// Settings menu class
class SettingsMenu {
public:
    SettingsMenu();
    ~SettingsMenu();

    // Disable copy
    SettingsMenu(const SettingsMenu&) = delete;
    SettingsMenu& operator=(const SettingsMenu&) = delete;

    // Initialize with dependencies
    bool begin(ConfigManager* config_mgr, DisplayRenderer* display);

    // Open/close menu
    void open();
    void close();
    bool isOpen() const { return state_ != MenuState::CLOSED; }

    // Handle input events (call from main loop)
    void onKeyEvent(const KeyEvent& event);

    // Update and render (call from main loop when menu is open)
    void update();
    void render();

    // Get current state
    MenuState getState() const { return state_; }

    // Check if settings were modified
    bool isModified() const { return modified_; }

private:
    // Dependencies
    ConfigManager* config_mgr_;
    DisplayRenderer* display_;
    AppConfig config_copy_;  // Working copy of config

    // State
    MenuState state_;
    MenuCategory current_category_;
    int selected_item_;
    int scroll_offset_;
    bool modified_;

    // Editing state
    char edit_buffer_[128];
    size_t edit_cursor_pos_;
    bool edit_mode_;

    // Message display
    char message_buffer_[256];
    uint32_t message_timeout_;

    // WiFi scan results
    struct WiFiNetwork {
        String ssid;
        int8_t rssi;
        uint8_t encryption;
    };
    WiFiNetwork wifi_networks_[20];
    int wifi_scan_count_;
    bool wifi_scanning_;

    // Menu definitions
    static const MenuItem menu_items_[];
    static constexpr int MAX_MENU_ITEMS = 20;
    static constexpr int ITEMS_PER_PAGE = 6;

    // Private methods
    void buildMenuForCategory(MenuCategory cat);
    int getItemCountForCategory(MenuCategory cat) const;
    const MenuItem* getItem(int index) const;

    void navigateUp();
    void navigateDown();
    void selectItem();
    void goBack();

    void startEditing(const MenuItem* item);
    void updateEditing(const KeyEvent& event);
    void finishEditing(bool save);
    void cancelEditing();

    void insertEditChar(char c);
    void deleteEditChar();
    void moveEditCursorLeft();
    void moveEditCursorRight();

    void saveSettings();
    void resetToDefaults();
    void testConnection();
    void startWiFiScan();
    void showDeviceInfo();

    void showMessage(const char* msg, uint32_t timeout_ms = 2000);
    void clearMessage();

    void renderMainMenu();
    void renderEditScreen();
    void renderConfirmDialog();
    void renderMessage();
    void renderWiFiScan();
    void renderDeviceInfo();

    const char* getCategoryName(MenuCategory cat) const;
    const char* getValueDisplay(const MenuItem* item) const;
    void applyValue(const MenuItem* item);
};

} // namespace OpenClaw

#endif // OPENCLAW_SETTINGS_MENU_H
