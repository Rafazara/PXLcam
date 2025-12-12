/**
 * @file test_main.cpp
 * @brief PXLcam v1.2.0 Native Unit Tests
 * 
 * 100% hardware-independent tests using Unity framework.
 * All dependencies are mocked locally - no ESP32/Arduino headers.
 * 
 * Test Suites:
 * - State Machine transitions
 * - Settings serialization/defaults
 * - Menu Navigation logic
 * - Dithering algorithms
 * 
 * Run with: pio test -e test_native (requires gcc)
 *       or: pio test -e test_esp32 --without-uploading --without-testing (build only)
 * 
 * @version 1.2.0
 */

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <unity.h>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <vector>

// ============================================================================
// MOCK IMPLEMENTATIONS (100% Native - No Hardware Dependencies)
// ============================================================================

namespace mock {

// --- State Machine Mock ---
enum class State : uint8_t {
    BOOT = 0, IDLE, MENU, PREVIEW, CAPTURE, STATE_COUNT
};

enum class Event : uint8_t {
    NONE = 0, BOOT_COMPLETE, BUTTON_PRESS, BUTTON_LONG_PRESS,
    BUTTON_HOLD, MENU_SELECT, MENU_BACK, CAPTURE_COMPLETE, TIMEOUT, EVENT_COUNT
};

using StateHandler = std::function<void()>;
using EventHandler = std::function<State(Event)>;

struct StateConfig {
    StateHandler onEnter;
    StateHandler onExit;
    StateHandler onUpdate;
    EventHandler onEvent;
};

class StateMachine {
public:
    StateMachine() : currentState_(State::BOOT), previousState_(State::BOOT), running_(false) {}

    void registerState(State s, const StateConfig& cfg) { 
        configs_[s] = cfg; 
    }

    bool start(State initial) {
        if (configs_.find(initial) == configs_.end()) {
            return false;  // State not registered
        }
        currentState_ = initial;
        previousState_ = initial;
        running_ = true;
        if (configs_[initial].onEnter) {
            configs_[initial].onEnter();
        }
        return true;
    }

    void handleEvent(Event e) {
        if (!running_ || e == Event::NONE) return;
        if (configs_.find(currentState_) == configs_.end()) return;
        
        auto& cfg = configs_[currentState_];
        if (cfg.onEvent) {
            State next = cfg.onEvent(e);
            if (next != currentState_ && configs_.find(next) != configs_.end()) {
                transitionTo(next);
            }
        }
    }

    void update() {
        if (!running_) return;
        if (configs_.find(currentState_) != configs_.end() && configs_[currentState_].onUpdate) {
            configs_[currentState_].onUpdate();
        }
    }

    void transitionTo(State next) {
        if (!running_) return;
        if (configs_.find(next) == configs_.end()) return;
        
        if (configs_.find(currentState_) != configs_.end() && configs_[currentState_].onExit) {
            configs_[currentState_].onExit();
        }
        previousState_ = currentState_;
        currentState_ = next;
        if (configs_[next].onEnter) {
            configs_[next].onEnter();
        }
    }

    void stop() {
        if (!running_) return;
        if (configs_.find(currentState_) != configs_.end() && configs_[currentState_].onExit) {
            configs_[currentState_].onExit();
        }
        running_ = false;
    }

    State getCurrentState() const { return currentState_; }
    State getPreviousState() const { return previousState_; }
    bool isRunning() const { return running_; }

private:
    State currentState_;
    State previousState_;
    bool running_;
    std::map<State, StateConfig> configs_;
};

const char* stateToString(State s) {
    switch (s) {
        case State::BOOT: return "BOOT";
        case State::IDLE: return "IDLE";
        case State::MENU: return "MENU";
        case State::PREVIEW: return "PREVIEW";
        case State::CAPTURE: return "CAPTURE";
        default: return "UNKNOWN";
    }
}

const char* eventToString(Event e) {
    switch (e) {
        case Event::NONE: return "NONE";
        case Event::BOOT_COMPLETE: return "BOOT_COMPLETE";
        case Event::BUTTON_PRESS: return "BUTTON_PRESS";
        case Event::BUTTON_LONG_PRESS: return "BUTTON_LONG_PRESS";
        case Event::BUTTON_HOLD: return "BUTTON_HOLD";
        case Event::MENU_SELECT: return "MENU_SELECT";
        case Event::MENU_BACK: return "MENU_BACK";
        case Event::CAPTURE_COMPLETE: return "CAPTURE_COMPLETE";
        case Event::TIMEOUT: return "TIMEOUT";
        default: return "UNKNOWN";
    }
}

// --- Settings Mock ---
enum class CameraMode : uint8_t {
    STANDARD = 0, PIXEL_ART, RETRO, MONOCHROME, MODE_COUNT
};

enum class Palette : uint8_t {
    FULL_COLOR = 0, GAMEBOY, CGA, EGA, SEPIA, CUSTOM, PALETTE_COUNT
};

enum class CaptureStyle : uint8_t {
    NORMAL = 0, DITHERED, OUTLINE, POSTERIZED, STYLE_COUNT
};

struct PersistedSettings {
    CameraMode currentMode;
    Palette paletteId;
    uint8_t brightness;
    CaptureStyle captureStyle;
    int8_t lastExposure;

    static PersistedSettings defaults() {
        return {
            CameraMode::STANDARD,
            Palette::FULL_COLOR,
            200,
            CaptureStyle::NORMAL,
            0
        };
    }
    
    // Serialize to byte buffer
    size_t serialize(uint8_t* buf, size_t maxLen) const {
        if (maxLen < 5) return 0;
        buf[0] = static_cast<uint8_t>(currentMode);
        buf[1] = static_cast<uint8_t>(paletteId);
        buf[2] = brightness;
        buf[3] = static_cast<uint8_t>(captureStyle);
        buf[4] = static_cast<uint8_t>(lastExposure);
        return 5;
    }
    
    // Deserialize from byte buffer
    static PersistedSettings deserialize(const uint8_t* buf, size_t len) {
        if (len < 5) return defaults();
        return {
            static_cast<CameraMode>(buf[0]),
            static_cast<Palette>(buf[1]),
            buf[2],
            static_cast<CaptureStyle>(buf[3]),
            static_cast<int8_t>(buf[4])
        };
    }
    
    bool operator==(const PersistedSettings& other) const {
        return currentMode == other.currentMode &&
               paletteId == other.paletteId &&
               brightness == other.brightness &&
               captureStyle == other.captureStyle &&
               lastExposure == other.lastExposure;
    }
};

// --- Menu Mock ---
enum class MenuItemType : uint8_t {
    ACTION = 0, SUBMENU, TOGGLE, VALUE, BACK
};

enum class MenuResult : uint8_t {
    NONE = 0, SELECTED, BACK, EXIT
};

using MenuAction = std::function<void()>;

struct MenuItem {
    const char* label;
    const char* description;
    MenuItemType type;
    MenuAction action;
    int submenuId;
    bool enabled;

    static MenuItem createAction(const char* lbl, const char* desc, MenuAction act) {
        return {lbl, desc, MenuItemType::ACTION, act, -1, true};
    }
    static MenuItem createSubmenu(const char* lbl, const char* desc, int id) {
        return {lbl, desc, MenuItemType::SUBMENU, nullptr, id, true};
    }
    static MenuItem createBack(const char* lbl = "< Back") {
        return {lbl, "Return", MenuItemType::BACK, nullptr, -1, true};
    }
};

class MenuSystem {
public:
    static constexpr int MAIN_MENU_ID = 0;
    static constexpr int SETTINGS_MENU_ID = 1;

    MenuSystem() : open_(false), menuId_(-1), selectedIdx_(0) {}

    void init() {
        // Main menu
        menus_.push_back({MAIN_MENU_ID, "Main Menu", {
            MenuItem::createAction("Preview", "Start preview", nullptr),
            MenuItem::createSubmenu("Settings", "Configure", SETTINGS_MENU_ID),
            MenuItem::createAction("About", "Info", nullptr)
        }});
        // Settings menu
        menus_.push_back({SETTINGS_MENU_ID, "Settings", {
            MenuItem::createAction("Mode", "Change mode", nullptr),
            MenuItem::createAction("Palette", "Change colors", nullptr),
            MenuItem::createBack()
        }});
    }

    bool open(int menuId) {
        for (size_t i = 0; i < menus_.size(); i++) {
            if (menus_[i].id == menuId) {
                menuId_ = menuId;
                currentMenuIdx_ = static_cast<int>(i);
                selectedIdx_ = 0;
                open_ = true;
                menuStack_.push_back(menuId);
                return true;
            }
        }
        return false;
    }

    void close() {
        open_ = false;
        menuId_ = -1;
        menuStack_.clear();
    }

    bool isOpen() const { return open_; }
    int getCurrentMenuId() const { return menuId_; }
    int getSelectedIndex() const { return selectedIdx_; }

    size_t getItemCount() const {
        if (!open_ || currentMenuIdx_ < 0) return 0;
        return menus_[currentMenuIdx_].items.size();
    }

    void navigateNext() {
        if (!open_ || getItemCount() == 0) return;
        selectedIdx_ = (selectedIdx_ + 1) % static_cast<int>(getItemCount());
    }

    void navigatePrev() {
        if (!open_ || getItemCount() == 0) return;
        selectedIdx_ = (selectedIdx_ - 1 + static_cast<int>(getItemCount())) % static_cast<int>(getItemCount());
    }

    const MenuItem* getSelectedItem() const {
        if (!open_ || currentMenuIdx_ < 0) return nullptr;
        if (selectedIdx_ < 0 || selectedIdx_ >= static_cast<int>(menus_[currentMenuIdx_].items.size())) return nullptr;
        return &menus_[currentMenuIdx_].items[selectedIdx_];
    }

    const char* getCurrentTitle() const {
        if (!open_ || currentMenuIdx_ < 0) return "";
        return menus_[currentMenuIdx_].title;
    }

    MenuResult select() {
        if (!open_) return MenuResult::NONE;
        const MenuItem* item = getSelectedItem();
        if (!item) return MenuResult::NONE;

        switch (item->type) {
            case MenuItemType::ACTION:
                if (item->action) item->action();
                return MenuResult::SELECTED;
            case MenuItemType::SUBMENU:
                open(item->submenuId);
                return MenuResult::SELECTED;
            case MenuItemType::BACK:
                return back();
            default:
                return MenuResult::NONE;
        }
    }

    MenuResult back() {
        if (menuStack_.size() <= 1) {
            close();
            return MenuResult::EXIT;
        }
        menuStack_.pop_back();
        int prevId = menuStack_.back();
        menuStack_.pop_back();
        open(prevId);
        return MenuResult::BACK;
    }

private:
    struct MenuDef {
        int id;
        const char* title;
        std::vector<MenuItem> items;
    };

    bool open_;
    int menuId_;
    int currentMenuIdx_ = -1;
    int selectedIdx_;
    std::vector<MenuDef> menus_;
    std::vector<int> menuStack_;
};

// --- PxlcamSettings Mock (v1.2.0) ---
enum class StyleMode : uint8_t {
    NORMAL = 0, GAMEBOY = 1, NIGHT = 2, STYLE_COUNT
};

struct PxlcamSettings {
    StyleMode styleMode;
    bool nightMode;
    bool autoExposure;

    static PxlcamSettings defaults() {
        return {StyleMode::NORMAL, false, true};
    }

    size_t serialize(uint8_t* buf, size_t maxLen) const {
        if (maxLen < 3) return 0;
        buf[0] = static_cast<uint8_t>(styleMode);
        buf[1] = nightMode ? 1 : 0;
        buf[2] = autoExposure ? 1 : 0;
        return 3;
    }

    static PxlcamSettings deserialize(const uint8_t* buf, size_t len) {
        if (len < 3) return defaults();
        return {
            static_cast<StyleMode>(buf[0]),
            buf[1] != 0,
            buf[2] != 0
        };
    }

    bool operator==(const PxlcamSettings& other) const {
        return styleMode == other.styleMode &&
               nightMode == other.nightMode &&
               autoExposure == other.autoExposure;
    }
};

// --- Dithering Mock ---
enum class DitherMode : uint8_t {
    Threshold = 0, GameBoy = 1, FloydSteinberg = 2, Night = 3
};

class DitherEngine {
public:
    DitherEngine() : mode_(DitherMode::Threshold), initialized_(false) {}

    void init() { initialized_ = true; }
    bool isInitialized() const { return initialized_; }
    void setMode(DitherMode m) { mode_ = m; }
    DitherMode getMode() const { return mode_; }

    // Convert grayscale to 1-bit using threshold
    void threshold(const uint8_t* gray, int w, int h, uint8_t* out, uint8_t thresh = 128) {
        int total = w * h;
        int outBytes = (total + 7) / 8;
        memset(out, 0, outBytes);
        for (int i = 0; i < total; i++) {
            if (gray[i] >= thresh) {
                out[i / 8] |= (0x80 >> (i % 8));  // MSB first
            }
        }
    }

    // Self-test: verify threshold dithering works correctly
    bool selfTest() {
        uint8_t gray[8] = {0, 64, 128, 192, 255, 255, 0, 0};
        uint8_t out[1] = {0};
        threshold(gray, 8, 1, out, 128);
        // Pixels 2,3,4,5 are >= 128 -> bits at positions 2,3,4,5 (MSB first)
        // Bit pattern: 00111100 = 0x3C
        return out[0] == 0x3C;
    }

private:
    DitherMode mode_;
    bool initialized_;
};

} // namespace mock

// ============================================================================
// Unity Test Hooks
// ============================================================================

void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

// ============================================================================
// STATE MACHINE TESTS
// ============================================================================

static mock::StateMachine* g_sm = nullptr;
static int g_enterCount = 0;
static int g_exitCount = 0;
static int g_updateCount = 0;

void resetSmCounters() {
    g_enterCount = 0;
    g_exitCount = 0;
    g_updateCount = 0;
}

void test_sm_initial_state(void) {
    mock::StateMachine sm;
    TEST_ASSERT_EQUAL(mock::State::BOOT, sm.getCurrentState());
    TEST_ASSERT_FALSE(sm.isRunning());
}

void test_sm_start_unregistered_fails(void) {
    mock::StateMachine sm;
    bool started = sm.start(mock::State::IDLE);  // Not registered
    TEST_ASSERT_FALSE(started);
    TEST_ASSERT_FALSE(sm.isRunning());
}

void test_sm_register_and_start(void) {
    mock::StateMachine sm;
    resetSmCounters();

    mock::StateConfig config = {
        []() { g_enterCount++; },
        []() { g_exitCount++; },
        []() { g_updateCount++; },
        [](mock::Event) { return mock::State::BOOT; }
    };

    sm.registerState(mock::State::BOOT, config);
    bool started = sm.start(mock::State::BOOT);

    TEST_ASSERT_TRUE(started);
    TEST_ASSERT_TRUE(sm.isRunning());
    TEST_ASSERT_EQUAL(1, g_enterCount);
}

void test_sm_simple_transition(void) {
    mock::StateMachine sm;
    resetSmCounters();

    sm.registerState(mock::State::BOOT, {
        []() { g_enterCount++; },
        []() { g_exitCount++; },
        nullptr,
        [](mock::Event e) {
            if (e == mock::Event::BOOT_COMPLETE) return mock::State::IDLE;
            return mock::State::BOOT;
        }
    });

    sm.registerState(mock::State::IDLE, {
        []() { g_enterCount++; },
        []() { g_exitCount++; },
        nullptr,
        [](mock::Event) { return mock::State::IDLE; }
    });

    sm.start(mock::State::BOOT);
    TEST_ASSERT_EQUAL(mock::State::BOOT, sm.getCurrentState());
    TEST_ASSERT_EQUAL(1, g_enterCount);

    sm.handleEvent(mock::Event::BOOT_COMPLETE);

    TEST_ASSERT_EQUAL(mock::State::IDLE, sm.getCurrentState());
    TEST_ASSERT_EQUAL(mock::State::BOOT, sm.getPreviousState());
    TEST_ASSERT_EQUAL(1, g_exitCount);
    TEST_ASSERT_EQUAL(2, g_enterCount);
}

void test_sm_no_transition_on_same_state(void) {
    mock::StateMachine sm;
    resetSmCounters();

    sm.registerState(mock::State::IDLE, {
        []() { g_enterCount++; },
        []() { g_exitCount++; },
        nullptr,
        [](mock::Event) { return mock::State::IDLE; }  // Always same
    });

    sm.start(mock::State::IDLE);
    TEST_ASSERT_EQUAL(1, g_enterCount);

    sm.handleEvent(mock::Event::BUTTON_PRESS);

    TEST_ASSERT_EQUAL(mock::State::IDLE, sm.getCurrentState());
    TEST_ASSERT_EQUAL(0, g_exitCount);
    TEST_ASSERT_EQUAL(1, g_enterCount);  // No re-enter
}

void test_sm_chained_transitions(void) {
    mock::StateMachine sm;

    sm.registerState(mock::State::BOOT, {nullptr, nullptr, nullptr,
        [](mock::Event e) { return e == mock::Event::BOOT_COMPLETE ? mock::State::IDLE : mock::State::BOOT; }
    });
    sm.registerState(mock::State::IDLE, {nullptr, nullptr, nullptr,
        [](mock::Event e) { return e == mock::Event::BUTTON_PRESS ? mock::State::PREVIEW : mock::State::IDLE; }
    });
    sm.registerState(mock::State::PREVIEW, {nullptr, nullptr, nullptr,
        [](mock::Event e) { return e == mock::Event::BUTTON_PRESS ? mock::State::CAPTURE : mock::State::PREVIEW; }
    });
    sm.registerState(mock::State::CAPTURE, {nullptr, nullptr, nullptr,
        [](mock::Event) { return mock::State::CAPTURE; }
    });

    sm.start(mock::State::BOOT);

    sm.handleEvent(mock::Event::BOOT_COMPLETE);
    TEST_ASSERT_EQUAL(mock::State::IDLE, sm.getCurrentState());

    sm.handleEvent(mock::Event::BUTTON_PRESS);
    TEST_ASSERT_EQUAL(mock::State::PREVIEW, sm.getCurrentState());

    sm.handleEvent(mock::Event::BUTTON_PRESS);
    TEST_ASSERT_EQUAL(mock::State::CAPTURE, sm.getCurrentState());
}

void test_sm_update_calls_handler(void) {
    mock::StateMachine sm;
    resetSmCounters();

    sm.registerState(mock::State::IDLE, {
        nullptr, nullptr,
        []() { g_updateCount++; },
        nullptr
    });

    sm.start(mock::State::IDLE);

    sm.update();
    TEST_ASSERT_EQUAL(1, g_updateCount);
    sm.update();
    TEST_ASSERT_EQUAL(2, g_updateCount);
}

void test_sm_stop_calls_exit(void) {
    mock::StateMachine sm;
    resetSmCounters();

    sm.registerState(mock::State::IDLE, {
        nullptr,
        []() { g_exitCount++; },
        nullptr, nullptr
    });

    sm.start(mock::State::IDLE);
    TEST_ASSERT_TRUE(sm.isRunning());

    sm.stop();
    TEST_ASSERT_FALSE(sm.isRunning());
    TEST_ASSERT_EQUAL(1, g_exitCount);
}

void test_sm_menu_flow(void) {
    mock::StateMachine sm;

    sm.registerState(mock::State::IDLE, {nullptr, nullptr, nullptr,
        [](mock::Event e) {
            if (e == mock::Event::BUTTON_LONG_PRESS) return mock::State::MENU;
            if (e == mock::Event::BUTTON_PRESS) return mock::State::PREVIEW;
            return mock::State::IDLE;
        }
    });
    sm.registerState(mock::State::MENU, {nullptr, nullptr, nullptr,
        [](mock::Event e) {
            if (e == mock::Event::MENU_BACK || e == mock::Event::BUTTON_LONG_PRESS) return mock::State::IDLE;
            return mock::State::MENU;
        }
    });
    sm.registerState(mock::State::PREVIEW, {nullptr, nullptr, nullptr, nullptr});

    sm.start(mock::State::IDLE);

    sm.handleEvent(mock::Event::BUTTON_LONG_PRESS);
    TEST_ASSERT_EQUAL(mock::State::MENU, sm.getCurrentState());

    sm.handleEvent(mock::Event::MENU_BACK);
    TEST_ASSERT_EQUAL(mock::State::IDLE, sm.getCurrentState());
}

void test_sm_string_conversion(void) {
    TEST_ASSERT_EQUAL_STRING("BOOT", mock::stateToString(mock::State::BOOT));
    TEST_ASSERT_EQUAL_STRING("IDLE", mock::stateToString(mock::State::IDLE));
    TEST_ASSERT_EQUAL_STRING("MENU", mock::stateToString(mock::State::MENU));
    TEST_ASSERT_EQUAL_STRING("PREVIEW", mock::stateToString(mock::State::PREVIEW));
    TEST_ASSERT_EQUAL_STRING("CAPTURE", mock::stateToString(mock::State::CAPTURE));

    TEST_ASSERT_EQUAL_STRING("BUTTON_PRESS", mock::eventToString(mock::Event::BUTTON_PRESS));
    TEST_ASSERT_EQUAL_STRING("BOOT_COMPLETE", mock::eventToString(mock::Event::BOOT_COMPLETE));
}

// ============================================================================
// SETTINGS SERIALIZATION TESTS
// ============================================================================

void test_settings_defaults(void) {
    mock::PersistedSettings s = mock::PersistedSettings::defaults();

    TEST_ASSERT_EQUAL(mock::CameraMode::STANDARD, s.currentMode);
    TEST_ASSERT_EQUAL(mock::Palette::FULL_COLOR, s.paletteId);
    TEST_ASSERT_EQUAL(200, s.brightness);
    TEST_ASSERT_EQUAL(mock::CaptureStyle::NORMAL, s.captureStyle);
    TEST_ASSERT_EQUAL(0, s.lastExposure);
}

void test_settings_serialize_deserialize(void) {
    mock::PersistedSettings original = {
        mock::CameraMode::PIXEL_ART,
        mock::Palette::GAMEBOY,
        150,
        mock::CaptureStyle::DITHERED,
        -2
    };

    uint8_t buffer[16];
    size_t written = original.serialize(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(5, written);

    mock::PersistedSettings restored = mock::PersistedSettings::deserialize(buffer, written);

    TEST_ASSERT_TRUE(original == restored);
}

void test_settings_serialize_insufficient_buffer(void) {
    mock::PersistedSettings s = mock::PersistedSettings::defaults();
    uint8_t buffer[2];  // Too small
    size_t written = s.serialize(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(0, written);
}

void test_settings_deserialize_insufficient_data(void) {
    uint8_t buffer[2] = {1, 2};
    mock::PersistedSettings s = mock::PersistedSettings::deserialize(buffer, 2);
    
    // Should return defaults on insufficient data
    mock::PersistedSettings defaults = mock::PersistedSettings::defaults();
    TEST_ASSERT_TRUE(s == defaults);
}

void test_settings_all_modes_valid(void) {
    for (int i = 0; i < static_cast<int>(mock::CameraMode::MODE_COUNT); i++) {
        mock::PersistedSettings s = mock::PersistedSettings::defaults();
        s.currentMode = static_cast<mock::CameraMode>(i);
        
        uint8_t buf[8];
        size_t len = s.serialize(buf, sizeof(buf));
        mock::PersistedSettings r = mock::PersistedSettings::deserialize(buf, len);
        
        TEST_ASSERT_EQUAL(s.currentMode, r.currentMode);
    }
}

void test_settings_all_palettes_valid(void) {
    for (int i = 0; i < static_cast<int>(mock::Palette::PALETTE_COUNT); i++) {
        mock::PersistedSettings s = mock::PersistedSettings::defaults();
        s.paletteId = static_cast<mock::Palette>(i);
        
        uint8_t buf[8];
        size_t len = s.serialize(buf, sizeof(buf));
        mock::PersistedSettings r = mock::PersistedSettings::deserialize(buf, len);
        
        TEST_ASSERT_EQUAL(s.paletteId, r.paletteId);
    }
}

void test_settings_exposure_range(void) {
    for (int8_t exp = -2; exp <= 2; exp++) {
        mock::PersistedSettings s = mock::PersistedSettings::defaults();
        s.lastExposure = exp;
        
        uint8_t buf[8];
        size_t len = s.serialize(buf, sizeof(buf));
        mock::PersistedSettings r = mock::PersistedSettings::deserialize(buf, len);
        
        TEST_ASSERT_EQUAL(exp, r.lastExposure);
    }
}

// ============================================================================
// PXLCAM SETTINGS (v1.2.0) TESTS
// ============================================================================

void test_pxlcam_settings_defaults(void) {
    mock::PxlcamSettings s = mock::PxlcamSettings::defaults();

    TEST_ASSERT_EQUAL(mock::StyleMode::NORMAL, s.styleMode);
    TEST_ASSERT_FALSE(s.nightMode);
    TEST_ASSERT_TRUE(s.autoExposure);
}

void test_pxlcam_settings_serialize_deserialize(void) {
    mock::PxlcamSettings original = {
        mock::StyleMode::GAMEBOY,
        true,
        false
    };

    uint8_t buffer[8];
    size_t written = original.serialize(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(3, written);

    mock::PxlcamSettings restored = mock::PxlcamSettings::deserialize(buffer, written);

    TEST_ASSERT_TRUE(original == restored);
    TEST_ASSERT_EQUAL(mock::StyleMode::GAMEBOY, restored.styleMode);
    TEST_ASSERT_TRUE(restored.nightMode);
    TEST_ASSERT_FALSE(restored.autoExposure);
}

void test_pxlcam_settings_serialize_insufficient_buffer(void) {
    mock::PxlcamSettings s = mock::PxlcamSettings::defaults();
    uint8_t buffer[2];  // Too small
    size_t written = s.serialize(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(0, written);
}

void test_pxlcam_settings_deserialize_insufficient_data(void) {
    uint8_t buffer[1] = {1};
    mock::PxlcamSettings s = mock::PxlcamSettings::deserialize(buffer, 1);
    
    // Should return defaults on insufficient data
    mock::PxlcamSettings defaults = mock::PxlcamSettings::defaults();
    TEST_ASSERT_TRUE(s == defaults);
}

void test_pxlcam_settings_all_style_modes(void) {
    for (int i = 0; i < static_cast<int>(mock::StyleMode::STYLE_COUNT); i++) {
        mock::PxlcamSettings s = mock::PxlcamSettings::defaults();
        s.styleMode = static_cast<mock::StyleMode>(i);
        
        uint8_t buf[8];
        size_t len = s.serialize(buf, sizeof(buf));
        mock::PxlcamSettings r = mock::PxlcamSettings::deserialize(buf, len);
        
        TEST_ASSERT_EQUAL(s.styleMode, r.styleMode);
    }
}

void test_pxlcam_settings_bool_combinations(void) {
    // Test all combinations of nightMode and autoExposure
    bool bools[] = {false, true};
    
    for (bool nightMode : bools) {
        for (bool autoExposure : bools) {
            mock::PxlcamSettings s = {mock::StyleMode::NORMAL, nightMode, autoExposure};
            
            uint8_t buf[8];
            size_t len = s.serialize(buf, sizeof(buf));
            mock::PxlcamSettings r = mock::PxlcamSettings::deserialize(buf, len);
            
            TEST_ASSERT_EQUAL(nightMode, r.nightMode);
            TEST_ASSERT_EQUAL(autoExposure, r.autoExposure);
        }
    }
}

void test_pxlcam_settings_night_mode_style(void) {
    mock::PxlcamSettings s = {mock::StyleMode::NIGHT, true, false};
    
    uint8_t buf[8];
    s.serialize(buf, sizeof(buf));
    mock::PxlcamSettings r = mock::PxlcamSettings::deserialize(buf, 3);
    
    TEST_ASSERT_EQUAL(mock::StyleMode::NIGHT, r.styleMode);
    TEST_ASSERT_TRUE(r.nightMode);
}

// ============================================================================
// MENU NAVIGATION TESTS
// ============================================================================

void test_menu_initial_state(void) {
    mock::MenuSystem menu;
    TEST_ASSERT_FALSE(menu.isOpen());
    TEST_ASSERT_EQUAL(-1, menu.getCurrentMenuId());
}

void test_menu_open_main(void) {
    mock::MenuSystem menu;
    menu.init();

    bool opened = menu.open(mock::MenuSystem::MAIN_MENU_ID);

    TEST_ASSERT_TRUE(opened);
    TEST_ASSERT_TRUE(menu.isOpen());
    TEST_ASSERT_EQUAL(mock::MenuSystem::MAIN_MENU_ID, menu.getCurrentMenuId());
    TEST_ASSERT_EQUAL(0, menu.getSelectedIndex());
}

void test_menu_open_invalid(void) {
    mock::MenuSystem menu;
    menu.init();

    bool opened = menu.open(999);

    TEST_ASSERT_FALSE(opened);
    TEST_ASSERT_FALSE(menu.isOpen());
}

void test_menu_navigate_next(void) {
    mock::MenuSystem menu;
    menu.init();
    menu.open(mock::MenuSystem::MAIN_MENU_ID);

    TEST_ASSERT_EQUAL(0, menu.getSelectedIndex());

    menu.navigateNext();
    TEST_ASSERT_EQUAL(1, menu.getSelectedIndex());

    menu.navigateNext();
    TEST_ASSERT_EQUAL(2, menu.getSelectedIndex());
}

void test_menu_navigate_wraps(void) {
    mock::MenuSystem menu;
    menu.init();
    menu.open(mock::MenuSystem::MAIN_MENU_ID);

    size_t count = menu.getItemCount();
    for (size_t i = 0; i < count; i++) {
        menu.navigateNext();
    }

    TEST_ASSERT_EQUAL(0, menu.getSelectedIndex());  // Wrapped
}

void test_menu_navigate_prev(void) {
    mock::MenuSystem menu;
    menu.init();
    menu.open(mock::MenuSystem::MAIN_MENU_ID);

    menu.navigateNext();
    menu.navigateNext();
    TEST_ASSERT_EQUAL(2, menu.getSelectedIndex());

    menu.navigatePrev();
    TEST_ASSERT_EQUAL(1, menu.getSelectedIndex());
}

void test_menu_navigate_prev_wraps(void) {
    mock::MenuSystem menu;
    menu.init();
    menu.open(mock::MenuSystem::MAIN_MENU_ID);

    TEST_ASSERT_EQUAL(0, menu.getSelectedIndex());

    menu.navigatePrev();
    TEST_ASSERT_EQUAL(static_cast<int>(menu.getItemCount()) - 1, menu.getSelectedIndex());
}

void test_menu_get_selected_item(void) {
    mock::MenuSystem menu;
    menu.init();
    menu.open(mock::MenuSystem::MAIN_MENU_ID);

    const mock::MenuItem* item = menu.getSelectedItem();

    TEST_ASSERT_NOT_NULL(item);
    TEST_ASSERT_EQUAL_STRING("Preview", item->label);
}

void test_menu_get_title(void) {
    mock::MenuSystem menu;
    menu.init();
    menu.open(mock::MenuSystem::MAIN_MENU_ID);

    TEST_ASSERT_EQUAL_STRING("Main Menu", menu.getCurrentTitle());
}

void test_menu_submenu_navigation(void) {
    mock::MenuSystem menu;
    menu.init();
    menu.open(mock::MenuSystem::MAIN_MENU_ID);

    menu.navigateNext();  // Settings
    mock::MenuResult result = menu.select();

    TEST_ASSERT_EQUAL(mock::MenuResult::SELECTED, result);
    TEST_ASSERT_EQUAL(mock::MenuSystem::SETTINGS_MENU_ID, menu.getCurrentMenuId());
    TEST_ASSERT_EQUAL_STRING("Settings", menu.getCurrentTitle());
}

void test_menu_back_navigation(void) {
    mock::MenuSystem menu;
    menu.init();
    menu.open(mock::MenuSystem::MAIN_MENU_ID);

    // Navigate to Settings submenu
    menu.navigateNext();
    menu.select();
    TEST_ASSERT_EQUAL(mock::MenuSystem::SETTINGS_MENU_ID, menu.getCurrentMenuId());

    // Navigate to Back item and select
    menu.navigateNext();
    menu.navigateNext();  // "< Back"
    mock::MenuResult result = menu.select();

    TEST_ASSERT_EQUAL(mock::MenuResult::BACK, result);
    TEST_ASSERT_EQUAL(mock::MenuSystem::MAIN_MENU_ID, menu.getCurrentMenuId());
}

void test_menu_close(void) {
    mock::MenuSystem menu;
    menu.init();
    menu.open(mock::MenuSystem::MAIN_MENU_ID);
    TEST_ASSERT_TRUE(menu.isOpen());

    menu.close();

    TEST_ASSERT_FALSE(menu.isOpen());
}

void test_menu_action_callback(void) {
    mock::MenuSystem menu;
    int actionCalled = 0;

    mock::MenuItem actionItem = mock::MenuItem::createAction("Test", "Desc", [&]() {
        actionCalled++;
    });

    // Manually invoke action
    if (actionItem.action) {
        actionItem.action();
    }

    TEST_ASSERT_EQUAL(1, actionCalled);
}

// ============================================================================
// DITHERING TESTS
// ============================================================================

void test_dither_init(void) {
    mock::DitherEngine dither;
    TEST_ASSERT_FALSE(dither.isInitialized());

    dither.init();
    TEST_ASSERT_TRUE(dither.isInitialized());
}

void test_dither_mode_get_set(void) {
    mock::DitherEngine dither;

    dither.setMode(mock::DitherMode::GameBoy);
    TEST_ASSERT_EQUAL(mock::DitherMode::GameBoy, dither.getMode());

    dither.setMode(mock::DitherMode::FloydSteinberg);
    TEST_ASSERT_EQUAL(mock::DitherMode::FloydSteinberg, dither.getMode());
}

void test_dither_threshold_all_black(void) {
    mock::DitherEngine dither;
    uint8_t gray[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t out[1] = {0xFF};

    dither.threshold(gray, 8, 1, out, 128);

    TEST_ASSERT_EQUAL_HEX8(0x00, out[0]);
}

void test_dither_threshold_all_white(void) {
    mock::DitherEngine dither;
    uint8_t gray[8] = {255, 255, 255, 255, 255, 255, 255, 255};
    uint8_t out[1] = {0x00};

    dither.threshold(gray, 8, 1, out, 128);

    TEST_ASSERT_EQUAL_HEX8(0xFF, out[0]);
}

void test_dither_threshold_pattern(void) {
    mock::DitherEngine dither;
    // Alternating: W B W B W B W B
    uint8_t gray[8] = {255, 0, 255, 0, 255, 0, 255, 0};
    uint8_t out[1] = {0x00};

    dither.threshold(gray, 8, 1, out, 128);

    // MSB first: 10101010 = 0xAA
    TEST_ASSERT_EQUAL_HEX8(0xAA, out[0]);
}

void test_dither_threshold_mid_gray(void) {
    mock::DitherEngine dither;
    uint8_t gray[8] = {128, 128, 128, 128, 128, 128, 128, 128};
    uint8_t out[1] = {0x00};

    dither.threshold(gray, 8, 1, out, 128);

    // 128 >= 128, so all white
    TEST_ASSERT_EQUAL_HEX8(0xFF, out[0]);
}

void test_dither_self_test(void) {
    mock::DitherEngine dither;
    TEST_ASSERT_TRUE(dither.selfTest());
}

void test_dither_output_size(void) {
    // 64x64 = 4096 pixels = 512 bytes output
    int w = 64, h = 64;
    int expectedBytes = (w * h + 7) / 8;
    TEST_ASSERT_EQUAL(512, expectedBytes);

    // 128x128 = 16384 pixels = 2048 bytes
    w = 128; h = 128;
    expectedBytes = (w * h + 7) / 8;
    TEST_ASSERT_EQUAL(2048, expectedBytes);
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

void runTests() {
    UNITY_BEGIN();

    // State Machine Tests
    RUN_TEST(test_sm_initial_state);
    RUN_TEST(test_sm_start_unregistered_fails);
    RUN_TEST(test_sm_register_and_start);
    RUN_TEST(test_sm_simple_transition);
    RUN_TEST(test_sm_no_transition_on_same_state);
    RUN_TEST(test_sm_chained_transitions);
    RUN_TEST(test_sm_update_calls_handler);
    RUN_TEST(test_sm_stop_calls_exit);
    RUN_TEST(test_sm_menu_flow);
    RUN_TEST(test_sm_string_conversion);

    // Settings Serialization Tests
    RUN_TEST(test_settings_defaults);
    RUN_TEST(test_settings_serialize_deserialize);
    RUN_TEST(test_settings_serialize_insufficient_buffer);
    RUN_TEST(test_settings_deserialize_insufficient_data);
    RUN_TEST(test_settings_all_modes_valid);
    RUN_TEST(test_settings_all_palettes_valid);
    RUN_TEST(test_settings_exposure_range);

    // PxlcamSettings (v1.2.0) Tests
    RUN_TEST(test_pxlcam_settings_defaults);
    RUN_TEST(test_pxlcam_settings_serialize_deserialize);
    RUN_TEST(test_pxlcam_settings_serialize_insufficient_buffer);
    RUN_TEST(test_pxlcam_settings_deserialize_insufficient_data);
    RUN_TEST(test_pxlcam_settings_all_style_modes);
    RUN_TEST(test_pxlcam_settings_bool_combinations);
    RUN_TEST(test_pxlcam_settings_night_mode_style);

    // Menu Navigation Tests
    RUN_TEST(test_menu_initial_state);
    RUN_TEST(test_menu_open_main);
    RUN_TEST(test_menu_open_invalid);
    RUN_TEST(test_menu_navigate_next);
    RUN_TEST(test_menu_navigate_wraps);
    RUN_TEST(test_menu_navigate_prev);
    RUN_TEST(test_menu_navigate_prev_wraps);
    RUN_TEST(test_menu_get_selected_item);
    RUN_TEST(test_menu_get_title);
    RUN_TEST(test_menu_submenu_navigation);
    RUN_TEST(test_menu_back_navigation);
    RUN_TEST(test_menu_close);
    RUN_TEST(test_menu_action_callback);

    // Dithering Tests
    RUN_TEST(test_dither_init);
    RUN_TEST(test_dither_mode_get_set);
    RUN_TEST(test_dither_threshold_all_black);
    RUN_TEST(test_dither_threshold_all_white);
    RUN_TEST(test_dither_threshold_pattern);
    RUN_TEST(test_dither_threshold_mid_gray);
    RUN_TEST(test_dither_self_test);
    RUN_TEST(test_dither_output_size);

    UNITY_END();
}

#ifdef ARDUINO
// Arduino/ESP32 entry point
void setup() {
    delay(2000);  // Wait for serial
    runTests();
}

void loop() {
    // Tests complete
}
#else
// Native entry point
int main(int argc, char** argv) {
    runTests();
    return 0;
}
#endif

