/**
 * @file main.cpp
 * @brief PXLcam Firmware Entry Point v1.2.0
 * 
 * Main entry point with new modular architecture:
 * - State Machine driven flow
 * - HAL abstraction with mocks
 * - Event-driven navigation
 * 
 * @author PXLcam Team
 * @version 1.2.0
 * @date 2024
 */

#include <Arduino.h>

// Legacy includes (preserved for compatibility)
#include "app_controller.h"
#include "preview.h"

// v1.2.0 Architecture includes
#include "core/state_machine.h"
#include "core/app_context.h"
#include "hal/hal_button.h"
#include "mocks/mock_button.h"
#include "features/menu_system.h"
#include "features/settings.h"
#include "features/capture_pipeline.h"
#include "ui/ui_theme.h"
#include "ui/ui_screens.h"

#ifndef PXLCAM_VERSION
#define PXLCAM_VERSION "1.2.0"
#endif

// ============================================================================
// Global Instances
// ============================================================================

// Legacy controller (preserved)
pxlcam::AppController legacyApp;

// v1.2.0 Core components
pxlcam::core::StateMachine stateMachine;
pxlcam::mocks::MockButton mockButton;
pxlcam::features::MenuSystem menuSystem;

// UI Screens
pxlcam::ui::SplashScreen splashScreen;
pxlcam::ui::IdleScreen idleScreen;
pxlcam::ui::MenuScreen* menuScreen = nullptr;
pxlcam::ui::PreviewScreen previewScreen;
pxlcam::ui::CaptureScreen captureScreen;

// Timing
uint32_t lastUpdateTime = 0;
constexpr uint32_t UPDATE_INTERVAL_MS = 16;  // ~60 FPS

// ============================================================================
// State Machine Configuration
// ============================================================================

/**
 * @brief Configure and register all states
 */
void setupStateMachine() {
    using namespace pxlcam::core;
    
    Serial.println("[Main] Configuring state machine...");
    
    // BOOT State
    stateMachine.registerState(State::BOOT, {
        .onEnter = []() {
            Serial.println("[State] BOOT: Initializing system...");
            pxlcam::ui::ScreenManager::instance().setScreen(pxlcam::ui::ScreenId::SPLASH);
        },
        .onExit = []() {
            Serial.println("[State] BOOT: Complete");
        },
        .onUpdate = []() {
            // Check if splash screen is complete (using static cast since RTTI is disabled)
            auto* screen = pxlcam::ui::ScreenManager::instance().getCurrentScreen();
            if (screen && screen->getId() == pxlcam::ui::ScreenId::SPLASH) {
                auto* splash = static_cast<pxlcam::ui::SplashScreen*>(screen);
                if (splash->isComplete()) {
                    stateMachine.handleEvent(Event::BOOT_COMPLETE);
                }
            }
        },
        .onEvent = [](Event e) -> State {
            if (e == Event::BOOT_COMPLETE) return State::IDLE;
            return State::BOOT;
        }
    });
    
    // IDLE State
    stateMachine.registerState(State::IDLE, {
        .onEnter = []() {
            Serial.println("[State] IDLE: Ready");
            pxlcam::ui::ScreenManager::instance().setScreen(pxlcam::ui::ScreenId::IDLE);
        },
        .onExit = []() {
            Serial.println("[State] IDLE: Exit");
        },
        .onUpdate = []() {
            // Idle state updates
        },
        .onEvent = [](Event e) -> State {
            switch (e) {
                case Event::BUTTON_PRESS:
                    menuSystem.open(pxlcam::features::MenuSystem::MAIN_MENU_ID);
                    return State::MENU;
                case Event::BUTTON_LONG_PRESS:
                    return State::PREVIEW;
                default:
                    return State::IDLE;
            }
        }
    });
    
    // MENU State - Single-button navigation:
    // - Short press (MENU_NAV): next item (wraps around)
    // - Long press 1s (MENU_SELECT): select item
    // - Hold 2s (BUTTON_HOLD): return to idle
    stateMachine.registerState(State::MENU, {
        .onEnter = []() {
            Serial.println("[State] MENU: Opened");
            pxlcam::ui::ScreenManager::instance().setScreen(pxlcam::ui::ScreenId::MENU);
        },
        .onExit = []() {
            Serial.println("[State] MENU: Closed");
            menuSystem.close();
        },
        .onUpdate = []() {
            // Menu updates handled by MenuSystem
        },
        .onEvent = [](Event e) -> State {
            switch (e) {
                case Event::MENU_NAV:
                    // Single-button: short press = next item
                    menuSystem.navigateNext();
                    return State::MENU;
                case Event::MENU_SELECT: {
                    auto result = menuSystem.select();
                    if (result == pxlcam::features::MenuResult::EXIT) {
                        return State::IDLE;
                    }
                    // Check if "Preview Mode" was selected
                    const auto* item = menuSystem.getSelectedItem();
                    if (item && strcmp(item->label, "Preview Mode") == 0) {
                        return State::PREVIEW;
                    }
                    // Check for "Reset Settings" action
                    if (item && strcmp(item->label, "Reset Settings") == 0) {
                        pxlcam::features::settings::loadDefaultValues(pxlcam::core::AppContext::instance());
                        pxlcam::features::settings::save(pxlcam::core::AppContext::instance());
                        Serial.println("[State] Settings reset to defaults");
                    }
                    return State::MENU;
                }
                case Event::MENU_BACK: {
                    auto result = menuSystem.back();
                    if (result == pxlcam::features::MenuResult::EXIT) {
                        return State::IDLE;
                    }
                    return State::MENU;
                }
                case Event::BUTTON_HOLD:
                    // Single-button: 2s hold = return to idle
                    Serial.println("[State] MENU: Hold detected - returning to IDLE");
                    return State::IDLE;
                default:
                    return State::MENU;
            }
        }
    });
    
    // PREVIEW State
    stateMachine.registerState(State::PREVIEW, {
        .onEnter = []() {
            Serial.println("[State] PREVIEW: Starting camera preview...");
            pxlcam::ui::ScreenManager::instance().setScreen(pxlcam::ui::ScreenId::PREVIEW);
            // Note: Actual camera preview uses legacy pxlcam::preview module
        },
        .onExit = []() {
            Serial.println("[State] PREVIEW: Stopped");
        },
        .onUpdate = []() {
            // Preview frame updates
        },
        .onEvent = [](Event e) -> State {
            switch (e) {
                case Event::BUTTON_PRESS:
                    return State::CAPTURE;
                case Event::BUTTON_LONG_PRESS:
                case Event::MENU_BACK:
                    return State::IDLE;
                default:
                    return State::PREVIEW;
            }
        }
    });
    
    // CAPTURE State - Stylized capture pipeline
    stateMachine.registerState(State::CAPTURE, {
        .onEnter = []() {
            Serial.println("[State] CAPTURE: Starting stylized capture pipeline...");
            pxlcam::ui::ScreenManager::instance().setScreen(pxlcam::ui::ScreenId::CAPTURE);
            
            // Run capture pipeline with current AppContext settings
            auto& ctx = pxlcam::core::AppContext::instance();
            auto result = pxlcam::features::capture::runCapture(ctx);
            
            if (result == pxlcam::features::capture::CaptureResult::SUCCESS) {
                Serial.println("[State] CAPTURE: Pipeline completed successfully");
                // Show confirmation screen with mini preview
                // (UI will display the mini preview from getLastPreview())
            } else {
                Serial.printf("[State] CAPTURE: Pipeline failed: %s\n",
                             pxlcam::features::capture::resultToString(result));
            }
            
            // Auto-complete after pipeline finishes
            stateMachine.handleEvent(pxlcam::core::Event::CAPTURE_COMPLETE);
        },
        .onExit = []() {
            Serial.println("[State] CAPTURE: Exiting");
        },
        .onUpdate = []() {
            // No periodic update needed - capture runs in onEnter
        },
        .onEvent = [](Event e) -> State {
            switch (e) {
                case Event::CAPTURE_COMPLETE:
                    // Return to IDLE after capture (confirmation shown)
                    return State::IDLE;
                case Event::BUTTON_PRESS:
                case Event::BUTTON_HOLD:
                    // Allow user to skip back to idle
                    return State::IDLE;
                default:
                    return State::CAPTURE;
            }
        }
    });
    
    Serial.println("[Main] State machine configured");
}

/**
 * @brief Initialize all v1.2.0 components
 */
void initializeV12Components() {
    Serial.println("[Main] Initializing v1.2.0 components...");
    
    // Initialize AppContext
    pxlcam::core::AppContext::instance().init();
    Serial.println("[Main] AppContext initialized");
    
    // Initialize NVS settings persistence
    if (pxlcam::features::settings::init()) {
        // Load settings from NVS into AppContext
        pxlcam::features::settings::load(pxlcam::core::AppContext::instance());
        
        if (pxlcam::features::settings::isFirstBoot()) {
            Serial.println("[Main] First boot - default settings applied");
        }
    } else {
        Serial.println("[Main] WARNING: NVS init failed, using defaults");
        pxlcam::features::settings::loadDefaultValues(pxlcam::core::AppContext::instance());
    }
    
    // Initialize mocks (for button simulation during development)
    mockButton.init();
    
    // Initialize menu system
    menuSystem.init();
    
    // Initialize UI theme
    pxlcam::ui::UiTheme::instance().init();
    
    // Initialize screen manager and register screens
    auto& screenMgr = pxlcam::ui::ScreenManager::instance();
    screenMgr.init();
    screenMgr.registerScreen(&splashScreen);
    screenMgr.registerScreen(&idleScreen);
    
    // Create menu screen with menu system reference
    menuScreen = new pxlcam::ui::MenuScreen(&menuSystem);
    screenMgr.registerScreen(menuScreen);
    screenMgr.registerScreen(&previewScreen);
    screenMgr.registerScreen(&captureScreen);
    
    Serial.println("[Main] v1.2.0 components initialized");
}

// ============================================================================
// Arduino Entry Points
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(100);  // Allow serial to stabilize
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("  PXLcam Firmware v" PXLCAM_VERSION);
    Serial.println("  ESP32-CAM Pixel Art Camera");
    Serial.println("  Architecture: v1.2.0 Modular");
    Serial.println("========================================");
    Serial.println();
    
    // Initialize v1.2.0 architecture
    initializeV12Components();
    setupStateMachine();
    
    // Start state machine in BOOT state
    stateMachine.start(pxlcam::core::State::BOOT);
    
    // Legacy initialization (preserved for hardware support)
    // Uncomment when hardware is available:
    // legacyApp.begin();
    // pxlcam::preview::begin();
    
    Serial.println("[Main] Setup complete");
    Serial.println();
}

void loop() {
    uint32_t now = millis();
    
    // Rate-limited updates
    if (now - lastUpdateTime >= UPDATE_INTERVAL_MS) {
        lastUpdateTime = now;
        
        // Update mock button (check for simulated events)
        mockButton.update();
        
        // Check for button events and convert to state machine events
        // Single-button navigation mapping:
        // - PRESS (short): In MENU -> MENU_NAV, In IDLE -> BUTTON_PRESS
        // - LONG_PRESS (1s): MENU_SELECT (select item)
        // - HOLD (2s): BUTTON_HOLD (return to idle from any state)
        auto buttonEvent = mockButton.getEvent(pxlcam::hal::ButtonId::SHUTTER);
        if (buttonEvent != pxlcam::hal::ButtonEvent::NONE) {
            auto currentState = stateMachine.getCurrentState();
            
            switch (buttonEvent) {
                case pxlcam::hal::ButtonEvent::PRESS:
                    if (currentState == pxlcam::core::State::MENU) {
                        // Short press in MENU = navigate to next item
                        stateMachine.handleEvent(pxlcam::core::Event::MENU_NAV);
                    } else {
                        // Short press elsewhere = general button press
                        stateMachine.handleEvent(pxlcam::core::Event::BUTTON_PRESS);
                    }
                    break;
                case pxlcam::hal::ButtonEvent::LONG_PRESS:
                    if (currentState == pxlcam::core::State::MENU) {
                        // Long press (1s) in MENU = select item
                        stateMachine.handleEvent(pxlcam::core::Event::MENU_SELECT);
                    } else {
                        // Long press elsewhere = enter preview/other actions
                        stateMachine.handleEvent(pxlcam::core::Event::BUTTON_LONG_PRESS);
                    }
                    break;
                case pxlcam::hal::ButtonEvent::HOLD:
                    // 2s hold = return to idle (from any state)
                    stateMachine.handleEvent(pxlcam::core::Event::BUTTON_HOLD);
                    break;
                case pxlcam::hal::ButtonEvent::DOUBLE_PRESS:
                    stateMachine.handleEvent(pxlcam::core::Event::BUTTON_DOUBLE_PRESS);
                    break;
                default:
                    break;
            }
        }
        
        // Update state machine
        stateMachine.update();
        
        // Update screen manager
        pxlcam::ui::ScreenManager::instance().update();
        pxlcam::ui::ScreenManager::instance().render();
    }
    
    // Legacy tick (uncomment when hardware is available)
    // legacyApp.tick();
    
    yield();
}