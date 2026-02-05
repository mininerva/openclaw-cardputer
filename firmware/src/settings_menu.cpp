/**
 * @file settings_menu.cpp
 * @brief Settings menu implementation
 */

#include "settings_menu.h"
#include "keyboard_handler.h"
#include <WiFi.h>
#include <M5Cardputer.h>

namespace OpenClaw {

// Menu item definitions
const MenuItem SettingsMenu::menu_items_[] = {
    // WiFi Settings
    {"SSID", MenuItemType::STRING, MenuCategory::WIFI,
     nullptr, 64, 0, 0, nullptr, nullptr, "WiFi network name"},
    {"Password", MenuItemType::STRING, MenuCategory::WIFI,
     nullptr, 64, 0, 0, nullptr, nullptr, "WiFi password"},
    {"Use DHCP", MenuItemType::BOOLEAN, MenuCategory::WIFI,
     nullptr, 0, 0, 0, nullptr, nullptr, "Auto IP assignment"},

    // Gateway Settings
    {"WebSocket URL", MenuItemType::STRING, MenuCategory::GATEWAY,
     nullptr, 128, 0, 0, nullptr, nullptr, "ws://host:port/path"},
    {"Fallback URL", MenuItemType::STRING, MenuCategory::GATEWAY,
     nullptr, 128, 0, 0, nullptr, nullptr, "http://host:port/api"},
    {"API Key", MenuItemType::STRING, MenuCategory::GATEWAY,
     nullptr, 64, 0, 0, nullptr, nullptr, "Optional auth key"},
    {"Reconnect (ms)", MenuItemType::INTEGER, MenuCategory::GATEWAY,
     nullptr, 0, 1000, 60000, nullptr, nullptr, "Reconnect interval"},

    // Device Settings
    {"Device ID", MenuItemType::STRING, MenuCategory::DEVICE,
     nullptr, 32, 0, 0, nullptr, nullptr, "Unique device ID"},
    {"Device Name", MenuItemType::STRING, MenuCategory::DEVICE,
     nullptr, 32, 0, 0, nullptr, nullptr, "Display name"},
    {"Brightness", MenuItemType::INTEGER, MenuCategory::DEVICE,
     nullptr, 0, 0, 255, nullptr, nullptr, "Screen brightness (0-255)"},
    {"Auto Connect", MenuItemType::BOOLEAN, MenuCategory::DEVICE,
     nullptr, 0, 0, 0, nullptr, nullptr, "Connect on boot"},

    // Audio Settings
    {"Sample Rate", MenuItemType::INTEGER, MenuCategory::AUDIO,
     nullptr, 0, 8000, 48000, nullptr, nullptr, "Hz (16000 recommended)"},
    {"Frame Duration", MenuItemType::INTEGER, MenuCategory::AUDIO,
     nullptr, 0, 20, 120, nullptr, nullptr, "ms (60 recommended)"},
    {"Codec", MenuItemType::ENUM, MenuCategory::AUDIO,
     nullptr, 0, 0, 0, nullptr, nullptr, "opus or pcm"},
    {"Mic Gain", MenuItemType::INTEGER, MenuCategory::AUDIO,
     nullptr, 0, 0, 100, nullptr, nullptr, "0-100 (64=neutral)"},

    // System Actions
    {"Save Settings", MenuItemType::ACTION, MenuCategory::SYSTEM,
     nullptr, 0, 0, 0, nullptr, nullptr, "Save to flash"},
    {"Test Connection", MenuItemType::ACTION, MenuCategory::SYSTEM,
     nullptr, 0, 0, 0, nullptr, nullptr, "Test WiFi + Gateway"},
    {"Factory Reset", MenuItemType::ACTION, MenuCategory::SYSTEM,
     nullptr, 0, 0, 0, nullptr, nullptr, "Wipe all settings"},

    // Tools
    {"WiFi Scan", MenuItemType::ACTION, MenuCategory::TOOLS,
     nullptr, 0, 0, 0, nullptr, nullptr, "Scan for networks"},
    {"Device Info", MenuItemType::ACTION, MenuCategory::TOOLS,
     nullptr, 0, 0, 0, nullptr, nullptr, "Show system info"},

    // Navigation
    {"Back", MenuItemType::ACTION, MenuCategory::SYSTEM,
     nullptr, 0, 0, 0, nullptr, nullptr, "Exit menu"},
};

SettingsMenu::SettingsMenu()
    : config_mgr_(nullptr), display_(nullptr),
      state_(MenuState::CLOSED), current_category_(MenuCategory::WIFI),
      selected_item_(0), scroll_offset_(0), modified_(false),
      edit_cursor_pos_(0), edit_mode_(false), message_timeout_(0),
      wifi_scan_count_(0), wifi_scanning_(false) {
    memset(edit_buffer_, 0, sizeof(edit_buffer_));
    memset(message_buffer_, 0, sizeof(message_buffer_));
    memset(wifi_networks_, 0, sizeof(wifi_networks_));
}

SettingsMenu::~SettingsMenu() {
}

bool SettingsMenu::begin(ConfigManager* config_mgr, DisplayRenderer* display) {
    config_mgr_ = config_mgr;
    display_ = display;

    if (config_mgr_) {
        config_copy_ = config_mgr_->getMutableConfig();
    }

    return true;
}

void SettingsMenu::open() {
    if (!config_mgr_ || !display_) return;

    // Reload config when opening
    config_copy_ = config_mgr_->getMutableConfig();
    state_ = MenuState::MAIN_MENU;
    current_category_ = MenuCategory::WIFI;
    selected_item_ = 0;
    scroll_offset_ = 0;
    modified_ = false;
    edit_mode_ = false;
    clearMessage();
}

void SettingsMenu::close() {
    state_ = MenuState::CLOSED;
    edit_mode_ = false;
}

void SettingsMenu::onKeyEvent(const KeyEvent& event) {
    if (state_ == MenuState::CLOSED) return;

    if (!event.pressed) return;  // Only handle press, not release

    // Handle special device info/WiFi scan dismissal
    int base_state = (int)state_;
    if (base_state >= (int)MenuState::SHOW_MESSAGE + 20) {
        // Device info view - any key closes it
        state_ = MenuState::MAIN_MENU;
        return;
    }
    if (base_state >= (int)MenuState::SHOW_MESSAGE + 10) {
        // WiFi scan view - navigate/select networks
        if (event.isNavigation()) {
            switch (event.special) {
                case SpecialKey::UP: navigateUp(); break;
                case SpecialKey::DOWN: navigateDown(); break;
                case SpecialKey::LEFT:
                case SpecialKey::ESCAPE:
                    state_ = MenuState::MAIN_MENU;
                    break;
                case SpecialKey::RIGHT:
                case SpecialKey::ENTER:
                    // Select network - copy SSID to WiFi config
                    if (wifi_scan_count_ > 0 && selected_item_ < wifi_scan_count_) {
                        config_copy_.wifi.ssid = wifi_networks_[selected_item_].ssid;
                        modified_ = true;
                        snprintf(message_buffer_, sizeof(message_buffer_), "Selected: %s",
                                wifi_networks_[selected_item_].ssid.c_str());
                        state_ = MenuState::SHOW_MESSAGE;
                        message_timeout_ = millis() + 1500;
                    }
                    break;
                default: break;
            }
        }
        return;
    }

    switch (state_) {
        case MenuState::MAIN_MENU:
            if (event.isNavigation()) {
                switch (event.special) {
                    case SpecialKey::UP: navigateUp(); break;
                    case SpecialKey::DOWN: navigateDown(); break;
                    case SpecialKey::LEFT: goBack(); break;
                    case SpecialKey::RIGHT:
                    case SpecialKey::ENTER: selectItem(); break;
                    default: break;
                }
            } else if (event.special == SpecialKey::ESCAPE ||
                       event.character == '`' || event.character == '~') {
                goBack();
            } else if (event.isEnter()) {
                selectItem();
            }
            break;

        case MenuState::EDIT_ITEM:
            updateEditing(event);
            break;

        case MenuState::CONFIRM_SAVE:
            if (event.character == 'y' || event.character == 'Y') {
                saveSettings();
                state_ = MenuState::MAIN_MENU;
            } else if (event.character == 'n' || event.character == 'N' ||
                       event.special == SpecialKey::ESCAPE) {
                state_ = MenuState::MAIN_MENU;
            }
            break;

        case MenuState::SHOW_MESSAGE:
            if (millis() > message_timeout_) {
                clearMessage();
                state_ = MenuState::MAIN_MENU;
            }
            break;

        default:
            break;
    }
}

void SettingsMenu::update() {
    if (state_ == MenuState::CLOSED) return;

    if (state_ == MenuState::SHOW_MESSAGE && millis() > message_timeout_) {
        clearMessage();
        state_ = MenuState::MAIN_MENU;
    }
}

void SettingsMenu::render() {
    if (state_ == MenuState::CLOSED) return;

    switch (state_) {
        case MenuState::MAIN_MENU:
            renderMainMenu();
            break;
        case MenuState::EDIT_ITEM:
            renderEditScreen();
            break;
        case MenuState::CONFIRM_SAVE:
            renderConfirmDialog();
            break;
        case MenuState::SHOW_MESSAGE:
            renderMessage();
            break;
        default:
            break;
    }
}

void SettingsMenu::navigateUp() {
    int count = getItemCountForCategory(current_category_);
    if (count <= 0) return;  // Guard against empty categories

    selected_item_--;
    if (selected_item_ < 0) selected_item_ = count - 1;

    // Adjust scroll
    if (selected_item_ < scroll_offset_) {
        scroll_offset_ = selected_item_;
    }
}

void SettingsMenu::navigateDown() {
    int count = getItemCountForCategory(current_category_);
    if (count <= 0) return;  // Guard against empty categories

    selected_item_++;
    if (selected_item_ >= count) selected_item_ = 0;

    // Adjust scroll
    if (selected_item_ >= scroll_offset_ + ITEMS_PER_PAGE) {
        scroll_offset_ = selected_item_ - ITEMS_PER_PAGE + 1;
    }
}

void SettingsMenu::selectItem() {
    const MenuItem* item = getItem(selected_item_);
    if (!item) return;

    switch (item->type) {
        case MenuItemType::STRING:
        case MenuItemType::INTEGER:
        case MenuItemType::BOOLEAN:
        case MenuItemType::ENUM:
            startEditing(item);
            break;

        case MenuItemType::ACTION:
            if (strcmp(item->label, "Save Settings") == 0) {
                if (modified_) {
                    state_ = MenuState::CONFIRM_SAVE;
                } else {
                    showMessage("No changes to save");
                }
            } else if (strcmp(item->label, "Test Connection") == 0) {
                testConnection();
            } else if (strcmp(item->label, "Factory Reset") == 0) {
                resetToDefaults();
            } else if (strcmp(item->label, "WiFi Scan") == 0) {
                startWiFiScan();
            } else if (strcmp(item->label, "Device Info") == 0) {
                showDeviceInfo();
            } else if (strcmp(item->label, "Back") == 0) {
                goBack();
            }
            break;

        case MenuItemType::SUBMENU:
            // Not used in current design
            break;
    }
}

void SettingsMenu::goBack() {
    if (state_ == MenuState::MAIN_MENU) {
        if (modified_) {
            state_ = MenuState::CONFIRM_SAVE;
        } else {
            close();
        }
    } else {
        state_ = MenuState::MAIN_MENU;
    }
}

void SettingsMenu::startEditing(const MenuItem* item) {
    if (!item) return;

    // Load current value into edit buffer
    const char* current = getValueDisplay(item);
    strncpy(edit_buffer_, current, sizeof(edit_buffer_) - 1);
    edit_buffer_[sizeof(edit_buffer_) - 1] = '\0';
    edit_cursor_pos_ = strlen(edit_buffer_);
    edit_mode_ = true;
    state_ = MenuState::EDIT_ITEM;
}

void SettingsMenu::updateEditing(const KeyEvent& event) {
    if (event.special == SpecialKey::ESCAPE) {
        cancelEditing();
        return;
    }

    if (event.isEnter()) {
        finishEditing(true);
        return;
    }

    if (event.isBackspace()) {
        deleteEditChar();
        return;
    }

    if (event.isNavigation()) {
        switch (event.special) {
            case SpecialKey::LEFT: moveEditCursorLeft(); break;
            case SpecialKey::RIGHT: moveEditCursorRight(); break;
            case SpecialKey::HOME: edit_cursor_pos_ = 0; break;
            case SpecialKey::END: edit_cursor_pos_ = strlen(edit_buffer_); break;
            default: break;
        }
        return;
    }

    if (event.isPrintable()) {
        insertEditChar(event.character);
    }
}

void SettingsMenu::finishEditing(bool save) {
    if (save) {
        // Apply the edited value to config_copy_
        const MenuItem* item = getItem(selected_item_);
        if (item) {
            applyValue(item);
            modified_ = true;
        }
    }

    edit_mode_ = false;
    state_ = MenuState::MAIN_MENU;
}

void SettingsMenu::cancelEditing() {
    edit_mode_ = false;
    state_ = MenuState::MAIN_MENU;
}

void SettingsMenu::insertEditChar(char c) {
    size_t len = strlen(edit_buffer_);
    if (len >= sizeof(edit_buffer_) - 2) return;  // Leave space for null terminator

    // Only allow printable ASCII
    if (c < 32 || c > 126) return;

    // Shift characters to make room
    for (size_t i = len + 1; i > edit_cursor_pos_; i--) {
        edit_buffer_[i] = edit_buffer_[i - 1];
    }

    edit_buffer_[edit_cursor_pos_++] = c;
}

void SettingsMenu::deleteEditChar() {
    if (edit_cursor_pos_ == 0) return;

    size_t len = strlen(edit_buffer_);

    // Shift characters left
    for (size_t i = edit_cursor_pos_ - 1; i < len; i++) {
        edit_buffer_[i] = edit_buffer_[i + 1];
    }

    edit_cursor_pos_--;
}

void SettingsMenu::moveEditCursorLeft() {
    if (edit_cursor_pos_ > 0) edit_cursor_pos_--;
}

void SettingsMenu::moveEditCursorRight() {
    if (edit_cursor_pos_ < strlen(edit_buffer_)) edit_cursor_pos_++;
}

void SettingsMenu::saveSettings() {
    if (!config_mgr_) return;

    // Apply working config back to config manager
    config_mgr_->getMutableConfig() = config_copy_;

    if (config_mgr_->save()) {
        modified_ = false;
        showMessage("Settings saved!");
    } else {
        showMessage("Save failed!");
    }
}

void SettingsMenu::resetToDefaults() {
    if (!config_mgr_) return;  // Guard against null

    ConfigManager temp;
    temp.resetToDefaults();
    config_copy_ = temp.getMutableConfig();
    modified_ = true;
    showMessage("Defaults restored");
}

void SettingsMenu::testConnection() {
    if (!display_) return;
    
    // Show testing message
    state_ = MenuState::SHOW_MESSAGE;
    snprintf(message_buffer_, sizeof(message_buffer_), "Testing...");
    message_timeout_ = millis() + 3000;
    
    // In a real implementation, this would:
    // 1. Test WiFi connection
    // 2. Test WebSocket connection
    // 3. Show success/error
    
    // For now, just show placeholder
    // The actual connection testing happens in main loop via WebSocket callbacks
}

void SettingsMenu::startWiFiScan() {
    if (!display_) return;

    // Start scan
    state_ = MenuState::SHOW_MESSAGE;
    wifi_scanning_ = true;
    wifi_scan_count_ = 0;
    snprintf(message_buffer_, sizeof(message_buffer_), "Scanning...");
    message_timeout_ = 0;  // Don't auto-dismiss

    // Trigger scan
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int16_t result = WiFi.scanNetworks(true, true, false, 200);

    if (result >= 0) {
        wifi_scan_count_ = (result < 20) ? result : 20;
        for (int i = 0; i < wifi_scan_count_; i++) {
            wifi_networks_[i].ssid = WiFi.SSID(i);
            wifi_networks_[i].rssi = WiFi.RSSI(i);
            wifi_networks_[i].encryption = WiFi.encryptionType(i);
        }

        selected_item_ = 0;
        scroll_offset_ = 0;
        // Use SHOW_MESSAGE as base, offset for special views
        state_ = (MenuState)((int)MenuState::SHOW_MESSAGE + 10);  // WiFi scan results
    } else {
        snprintf(message_buffer_, sizeof(message_buffer_), "Scan failed: %d", result);
        message_timeout_ = millis() + 2000;
    }

    WiFi.scanDelete();
    wifi_scanning_ = false;
}

void SettingsMenu::showDeviceInfo() {
    if (!display_) return;

    // Build info string
    char info[512];
    int pos = 0;

    pos += snprintf(info + pos, sizeof(info) - pos, "OpenClaw Cardputer\n");
    pos += snprintf(info + pos, sizeof(info) - pos, "FW: %s %s\n", FIRMWARE_VERSION, FIRMWARE_CODENAME);
    pos += snprintf(info + pos, sizeof(info) - pos, "IP: %s\n", WiFi.localIP().toString().c_str());
    pos += snprintf(info + pos, sizeof(info) - pos, "WiFi: %s (%d dBm)\n", WiFi.SSID().c_str(), WiFi.RSSI());
    pos += snprintf(info + pos, sizeof(info) - pos, "Uptime: %ld s\n", millis() / 1000);
    pos += snprintf(info + pos, sizeof(info) - pos, "RAM: %d / %d KB\n",
                 ESP.getFreeHeap() / 1024, ESP.getHeapSize() / 1024);
    pos += snprintf(info + pos, sizeof(info) - pos, "Flash: %d / %d KB\n",
                 ESP.getSketchSize() / 1024, ESP.getFlashChipSize() / 1024);

    // Show message with no timeout (requires key press to dismiss)
    state_ = (MenuState)((int)MenuState::SHOW_MESSAGE + 20);  // Device info
    strncpy(message_buffer_, info, sizeof(message_buffer_) - 1);
    message_timeout_ = 0;
}

void SettingsMenu::showMessage(const char* msg, uint32_t timeout_ms) {
    strncpy(message_buffer_, msg, sizeof(message_buffer_) - 1);
    message_buffer_[sizeof(message_buffer_) - 1] = '\0';
    message_timeout_ = millis() + timeout_ms;
    state_ = MenuState::SHOW_MESSAGE;
}

void SettingsMenu::clearMessage() {
    message_buffer_[0] = '\0';
    message_timeout_ = 0;
}

int SettingsMenu::getItemCountForCategory(MenuCategory cat) const {
    int count = 0;
    for (const auto& item : menu_items_) {
        if (item.category == cat) count++;
    }
    return count;
}

const MenuItem* SettingsMenu::getItem(int index) const {
    int count = 0;
    for (const auto& item : menu_items_) {
        if (item.category == current_category_) {
            if (count == index) return &item;
            count++;
        }
    }
    return nullptr;
}

const char* SettingsMenu::getCategoryName(MenuCategory cat) const {
    switch (cat) {
        case MenuCategory::WIFI: return "WiFi Settings";
        case MenuCategory::GATEWAY: return "Gateway Settings";
        case MenuCategory::DEVICE: return "Device Settings";
        case MenuCategory::AUDIO: return "Audio Settings";
        case MenuCategory::SYSTEM: return "System";
        case MenuCategory::TOOLS: return "Tools";
        default: return "Unknown";
    }
}

const char* SettingsMenu::getValueDisplay(const MenuItem* item) const {
    static char buffer[64];

    if (!item) return "";

    switch (item->category) {
        case MenuCategory::WIFI:
            if (strcmp(item->label, "SSID") == 0) {
                return config_copy_.wifi.ssid.c_str();
            } else if (strcmp(item->label, "Password") == 0) {
                return config_copy_.wifi.password.length() > 0 ? "********" : "";
            } else if (strcmp(item->label, "Use DHCP") == 0) {
                return config_copy_.wifi.dhcp ? "Yes" : "No";
            }
            break;

        case MenuCategory::GATEWAY:
            if (strcmp(item->label, "WebSocket URL") == 0) {
                return config_copy_.gateway.websocket_url.c_str();
            } else if (strcmp(item->label, "Fallback URL") == 0) {
                return config_copy_.gateway.fallback_http_url.c_str();
            } else if (strcmp(item->label, "API Key") == 0) {
                return config_copy_.gateway.api_key.length() > 0 ? "********" : "";
            } else if (strcmp(item->label, "Reconnect (ms)") == 0) {
                snprintf(buffer, sizeof(buffer), "%d", config_copy_.gateway.reconnect_interval_ms);
                return buffer;
            }
            break;

        case MenuCategory::DEVICE:
            if (strcmp(item->label, "Device ID") == 0) {
                return config_copy_.device.id.c_str();
            } else if (strcmp(item->label, "Device Name") == 0) {
                return config_copy_.device.name.c_str();
            } else if (strcmp(item->label, "Brightness") == 0) {
                snprintf(buffer, sizeof(buffer), "%d", config_copy_.device.display_brightness);
                return buffer;
            } else if (strcmp(item->label, "Auto Connect") == 0) {
                return config_copy_.device.auto_connect ? "Yes" : "No";
            }
            break;

        case MenuCategory::AUDIO:
            if (strcmp(item->label, "Sample Rate") == 0) {
                snprintf(buffer, sizeof(buffer), "%d", config_copy_.audio.sample_rate);
                return buffer;
            } else if (strcmp(item->label, "Frame Duration") == 0) {
                snprintf(buffer, sizeof(buffer), "%d", config_copy_.audio.frame_duration_ms);
                return buffer;
            } else if (strcmp(item->label, "Codec") == 0) {
                return config_copy_.audio.codec.c_str();
            } else if (strcmp(item->label, "Mic Gain") == 0) {
                snprintf(buffer, sizeof(buffer), "%d", config_copy_.audio.mic_gain);
                return buffer;
            }
            break;

        default:
            break;
    }

    return "";
}

void SettingsMenu::applyValue(const MenuItem* item) {
    if (!item) return;

    switch (item->category) {
        case MenuCategory::WIFI:
            if (strcmp(item->label, "SSID") == 0) {
                config_copy_.wifi.ssid = edit_buffer_;
            } else if (strcmp(item->label, "Password") == 0) {
                config_copy_.wifi.password = edit_buffer_;
            } else if (strcmp(item->label, "Use DHCP") == 0) {
                config_copy_.wifi.dhcp = (strcmp(edit_buffer_, "Yes") == 0 ||
                                          strcmp(edit_buffer_, "yes") == 0 ||
                                          strcmp(edit_buffer_, "true") == 0 ||
                                          strcmp(edit_buffer_, "1") == 0);
            }
            break;

        case MenuCategory::GATEWAY:
            if (strcmp(item->label, "WebSocket URL") == 0) {
                config_copy_.gateway.websocket_url = edit_buffer_;
            } else if (strcmp(item->label, "Fallback URL") == 0) {
                config_copy_.gateway.fallback_http_url = edit_buffer_;
            } else if (strcmp(item->label, "API Key") == 0) {
                config_copy_.gateway.api_key = edit_buffer_;
            } else if (strcmp(item->label, "Reconnect (ms)") == 0) {
                config_copy_.gateway.reconnect_interval_ms = atoi(edit_buffer_);
            }
            break;

        case MenuCategory::DEVICE:
            if (strcmp(item->label, "Device ID") == 0) {
                config_copy_.device.id = edit_buffer_;
            } else if (strcmp(item->label, "Device Name") == 0) {
                config_copy_.device.name = edit_buffer_;
            } else if (strcmp(item->label, "Brightness") == 0) {
                config_copy_.device.display_brightness = atoi(edit_buffer_);
            } else if (strcmp(item->label, "Auto Connect") == 0) {
                config_copy_.device.auto_connect = (strcmp(edit_buffer_, "Yes") == 0 ||
                                                    strcmp(edit_buffer_, "yes") == 0 ||
                                                    strcmp(edit_buffer_, "true") == 0 ||
                                                    strcmp(edit_buffer_, "1") == 0);
            }
            break;

        case MenuCategory::AUDIO:
            if (strcmp(item->label, "Sample Rate") == 0) {
                config_copy_.audio.sample_rate = atoi(edit_buffer_);
            } else if (strcmp(item->label, "Frame Duration") == 0) {
                config_copy_.audio.frame_duration_ms = atoi(edit_buffer_);
            } else if (strcmp(item->label, "Codec") == 0) {
                config_copy_.audio.codec = edit_buffer_;
            } else if (strcmp(item->label, "Mic Gain") == 0) {
                config_copy_.audio.mic_gain = atoi(edit_buffer_);
            }
            break;

        default:
            break;
    }
}

void SettingsMenu::renderMainMenu() {
    if (!display_) return;

    auto& canvas = M5Cardputer.Display;
    canvas.fillScreen(TFT_BLACK);

    // Title bar
    canvas.fillRect(0, 0, 240, 20, 0x1082);
    canvas.setTextColor(TFT_WHITE, 0x1082);
    canvas.setTextSize(1);
    canvas.setCursor(4, 4);
    canvas.printf("Settings: %s", getCategoryName(current_category_));

    if (modified_) {
        canvas.setCursor(200, 4);
        canvas.print("*");
    }

    // Category tabs
    const char* cats[] = {"WiFi", "GW", "Dev", "Aud", "Sys", "Tools"};
    int cat_x = 4;
    for (int i = 0; i < 6; i++) {
        uint16_t bg = (i == (int)current_category_) ? 0x07E0 : 0x8410;
        uint16_t fg = (i == (int)current_category_) ? TFT_BLACK : TFT_WHITE;
        int w = (i == 1 || i == 5) ? 28 : 36;  // "GW" and "Tools" are shorter

        canvas.fillRoundRect(cat_x, 24, w, 14, 2, bg);
        canvas.setTextColor(fg, bg);
        canvas.setCursor(cat_x + 4, 26);
        canvas.print(cats[i]);
        cat_x += w + 2;
    }

    // Menu items
    int y = 44;
    int count = getItemCountForCategory(current_category_);

    for (int i = scroll_offset_; i < count && i < scroll_offset_ + ITEMS_PER_PAGE; i++) {
        const MenuItem* item = getItem(i);
        if (!item) continue;

        bool selected = (i == selected_item_);
        uint16_t bg = selected ? 0xFFE0 : TFT_BLACK;
        uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;

        // Selection highlight
        if (selected) {
            canvas.fillRect(0, y - 2, 240, 16, bg);
        }

        // Label
        canvas.setTextColor(fg, bg);
        canvas.setCursor(4, y);
        canvas.print(item->label);

        // Value
        const char* value = getValueDisplay(item);
        int val_len = strlen(value) * 6;
        canvas.setCursor(236 - val_len, y);
        canvas.print(value);

        y += 16;
    }

    // Help text
    const MenuItem* sel = getItem(selected_item_);
    if (sel && sel->help_text) {
        canvas.setTextColor(0x8410, TFT_BLACK);
        canvas.setCursor(4, 120);
        canvas.print(sel->help_text);
    }

    // Footer
    canvas.setTextColor(0x8410, TFT_BLACK);
    canvas.setCursor(4, 128);
    canvas.print("\x1E\x1F=nav \x11=edit ESC=back");
}

void SettingsMenu::renderEditScreen() {
    if (!display_) return;

    auto& canvas = M5Cardputer.Display;
    canvas.fillScreen(TFT_BLACK);

    const MenuItem* item = getItem(selected_item_);
    if (!item) return;

    // Title
    canvas.fillRect(0, 0, 240, 20, 0x1082);
    canvas.setTextColor(TFT_WHITE, 0x1082);
    canvas.setCursor(4, 4);
    canvas.print("Edit: ");
    canvas.print(item->label);

    // Current value display
    canvas.setTextColor(0x8410, TFT_BLACK);
    canvas.setCursor(4, 30);
    canvas.print("Current:");

    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setCursor(4, 46);
    canvas.print(getValueDisplay(item));

    // Edit box
    canvas.fillRect(4, 70, 232, 24, 0x2104);
    canvas.setTextColor(TFT_YELLOW, 0x2104);
    canvas.setCursor(8, 76);
    canvas.print(edit_buffer_);

    // Cursor
    int cursor_x = 8 + (edit_cursor_pos_ * 6);
    canvas.fillRect(cursor_x, 74, 6, 16, 0xFFE0);

    // Instructions
    canvas.setTextColor(0x8410, TFT_BLACK);
    canvas.setCursor(4, 110);
    canvas.print("ENTER=save ESC=cancel");
}

void SettingsMenu::renderConfirmDialog() {
    if (!display_) return;

    auto& canvas = M5Cardputer.Display;

    // Modal background
    canvas.fillRect(20, 30, 200, 75, 0x2104);
    canvas.drawRect(20, 30, 200, 75, TFT_WHITE);

    // Title
    canvas.setTextColor(TFT_WHITE, 0x2104);
    canvas.setCursor(30, 40);
    canvas.print("Save Changes?");

    // Message
    canvas.setTextColor(0x8410, 0x2104);
    canvas.setCursor(30, 58);
    canvas.print("Settings modified.");
    canvas.setCursor(30, 72);
    canvas.print("Save to flash?");

    // Options
    canvas.setTextColor(TFT_GREEN, 0x2104);
    canvas.setCursor(50, 92);
    canvas.print("Y = Yes");

    canvas.setTextColor(TFT_RED, 0x2104);
    canvas.setCursor(130, 92);
    canvas.print("N = No");
}

void SettingsMenu::renderMessage() {
    if (!display_) return;

    // Handle special views with larger offsets
    int base_state = (int)state_;
    if (base_state >= (int)MenuState::SHOW_MESSAGE + 10) {
        renderWiFiScan();
        return;
    }
    if (base_state >= (int)MenuState::SHOW_MESSAGE + 20) {
        renderDeviceInfo();
        return;
    }

    auto& canvas = M5Cardputer.Display;

    // Centered message box
    int msg_len = strlen(message_buffer_);
    int line_count = 1;
    for (int i = 0; i < msg_len; i++) {
        if (message_buffer_[i] == '\n') line_count++;
    }

    int box_w = min(220, (msg_len / line_count) * 6 + 20);
    int box_h = line_count * 12 + 10;
    int box_x = (240 - box_w) / 2;
    int box_y = (135 - box_h) / 2;

    canvas.fillRect(box_x, box_y, box_w, box_h, 0x2104);
    canvas.drawRect(box_x, box_y, box_w, box_h, TFT_WHITE);

    canvas.setTextColor(TFT_WHITE, 0x2104);
    int y = box_y + 8;
    char line[64];
    int line_pos = 0;

    for (int i = 0; i < msg_len; i++) {
        if (message_buffer_[i] == '\n' || line_pos >= 63) {
            line[line_pos] = '\0';
            canvas.setCursor(box_x + 10, y);
            canvas.print(line);
            y += 12;
            line_pos = 0;
        } else {
            line[line_pos++] = message_buffer_[i];
        }
    }

    if (line_pos > 0) {
        line[line_pos] = '\0';
        canvas.setCursor(box_x + 10, y);
        canvas.print(line);
    }
}

void SettingsMenu::renderWiFiScan() {
    if (!display_) return;

    auto& canvas = M5Cardputer.Display;
    canvas.fillScreen(TFT_BLACK);

    // Title
    canvas.fillRect(0, 0, 240, 20, 0x1082);
    canvas.setTextColor(TFT_WHITE, 0x1082);
    canvas.setCursor(4, 4);
    canvas.printf("WiFi Networks (%d)", wifi_scan_count_);

    // Header
    canvas.setTextColor(0x8410, TFT_BLACK);
    canvas.setCursor(4, 26);
    canvas.print("Network              RSSI Sec");

    // Networks
    int y = 38;
    for (int i = scroll_offset_; i < wifi_scan_count_ && i < scroll_offset_ + ITEMS_PER_PAGE; i++) {
        const auto& net = wifi_networks_[i];
        bool selected = (i == selected_item_);

        uint16_t bg = selected ? 0xFFE0 : TFT_BLACK;
        uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;

        if (selected) {
            canvas.fillRect(0, y - 2, 240, 12, bg);
        }

        canvas.setTextColor(fg, bg);
        canvas.setCursor(4, y);

        // SSID (truncate if needed)
        char ssid[22];
        strncpy(ssid, net.ssid.c_str(), 21);
        ssid[21] = '\0';
        canvas.printf("%-21s", ssid);

        // RSSI with color
        uint16_t rssi_color = (net.rssi > -50) ? 0x07E0 : (net.rssi > -70) ? 0xFFE0 : 0xF800;
        canvas.setTextColor(rssi_color, bg);
        canvas.printf("%4d", net.rssi);

        // Security
        const char* sec = (net.encryption == WIFI_AUTH_OPEN) ? "Open" : "WPA";
        canvas.setTextColor(fg, bg);
        canvas.printf(" %s", sec);

        y += 12;
    }

    // Instructions
    canvas.setTextColor(0x8410, TFT_BLACK);
    canvas.setCursor(4, 120);
    canvas.print("\x1E\x1F=nav ENTER=select ESC=back");
}

void SettingsMenu::renderDeviceInfo() {
    if (!display_) return;

    auto& canvas = M5Cardputer.Display;
    canvas.fillScreen(TFT_BLACK);

    // Title
    canvas.fillRect(0, 0, 240, 20, 0x1082);
    canvas.setTextColor(TFT_WHITE, 0x1082);
    canvas.setCursor(4, 4);
    canvas.print("Device Information");

    // Info text
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    char line[48];
    int y = 26;
    char* ptr = message_buffer_;

    while (*ptr && y < 130) {
        char* newline = strchr(ptr, '\n');
        if (newline) {
            *newline = '\0';
            strncpy(line, ptr, 47);
            line[47] = '\0';
            canvas.setCursor(4, y);
            canvas.print(line);
            ptr = newline + 1;
        } else {
            strncpy(line, ptr, 47);
            line[47] = '\0';
            canvas.setCursor(4, y);
            canvas.print(line);
            break;
        }
        y += 10;
    }

    // Instructions
    canvas.setTextColor(0x8410, TFT_BLACK);
    canvas.setCursor(4, 128);
    canvas.print("Any key to close");
}

} // namespace OpenClaw
