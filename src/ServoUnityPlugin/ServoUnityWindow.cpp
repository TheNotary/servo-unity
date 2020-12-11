//
// ServoUnityWindow.cpp
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0.If a copy of the MPL was not distributed with this
// file, You can obtain one at https ://mozilla.org/MPL/2.0/.
//
// Copyright (c) 2020, Eden Networks Ltd.
// Copyright (c) 2019-2020 Mozilla, Inc.
//
// Author(s): Philip Lamb
//

#include "ServoUnityWindow.h"
#include <stdlib.h>
#include "servo_unity_internal.h"
#include "servo_unity_log.h"
#include "utils.h"


// Unfortunately the simpleservo interface doesn't allow arbitrary userdata
// to be passed along with callbacks, so we have to keep a global static
// instance pointer so that we can correctly call back to the correct window
// instance.
ServoUnityWindow *ServoUnityWindow::s_servo = nullptr;

ServoUnityWindow::ServoUnityWindow(int uid, int uidExt) :
    m_uid(uid),
    m_uidExt(uidExt),
    m_windowCreatedCallback(nullptr),
    m_windowResizedCallback(nullptr),
    m_browserEventCallback(nullptr),
    m_updateContinuously(false),
    m_updateOnce(false),
    m_title(std::string()),
    m_URL(std::string()),
    m_waitingForShutdown(false)
{
}

bool ServoUnityWindow::init(PFN_WINDOWCREATEDCALLBACK windowCreatedCallback, PFN_WINDOWRESIZEDCALLBACK windowResizedCallback, PFN_BROWSEREVENTCALLBACK browserEventCallback)
{
    m_windowCreatedCallback = windowCreatedCallback;
    m_windowResizedCallback = windowResizedCallback;
    m_browserEventCallback = browserEventCallback;

	return true;
}

void ServoUnityWindow::requestUpdate(float timeDelta) {
    SERVOUNITYLOGd("ServoUnityWindow::requestUpdate(%f)\n", timeDelta);

    if (!s_servo) {
        SERVOUNITYLOGi("initing servo.\n");
        s_servo = this;
        
        // Note about logs:
        // By default: all modules are enabled. Only warn level-logs are displayed.
        // To change the log level, add e.g. "--vslogger-level debug" to .args.
        // To only print logs from specific modules, add their names to pfilters.
        // For example:
        // static char *pfilters[] = {
        //   "servo",
        //   "simpleservo",
        //   "script::dom::bindings::error", // Show JS errors by default.
        //   "canvas::webgl_thread", // Show GL errors by default.
        //   "compositing",
        //   "constellation",
        // };
        // .vslogger_mod_list = pfilters;
        // .vslogger_mod_size = sizeof(pfilters) / sizeof(pfilters[0]);
        const char *args = nullptr;
		std::string arg_ll = std::string();
		std::string arg_ll_debug = "debug";
		std::string arg_ll_info = "info";
		std::string arg_ll_warn = "warn";
		std::string arg_ll_error = "error";
        switch (servoUnityLogLevel) {
            case SERVO_UNITY_LOG_LEVEL_DEBUG: arg_ll = arg_ll_debug; break;
            case SERVO_UNITY_LOG_LEVEL_INFO: arg_ll = arg_ll_info; break;
            case SERVO_UNITY_LOG_LEVEL_WARN: arg_ll = arg_ll_warn; break;
            case SERVO_UNITY_LOG_LEVEL_ERROR: arg_ll = arg_ll_error; break;
            default: break;
        }
        if (!arg_ll.empty()) args = (std::string("--vslogger-level ") + arg_ll).c_str();

        CInitOptions cio {
            /*.args =*/ args,
            /*.width =*/ size().w,
            /*.height =*/ size().h,
            /*.density =*/ 1.0f,
            /*.vslogger_mod_list =*/ nullptr,
            /*.vslogger_mod_size =*/ 0,
            /*.native_widget =*/ nullptr,
			/*.prefs =*/ nullptr
        };
        CHostCallbacks chc {
            /*.on_load_started =*/ on_load_started,
            /*.on_load_ended =*/ on_load_ended,
            /*.on_title_changed =*/ on_title_changed,
            /*.on_allow_navigation =*/ on_allow_navigation,
            /*.on_url_changed =*/ on_url_changed,
            /*.on_history_changed =*/ on_history_changed,
            /*.on_animating_changed =*/ on_animating_changed,
            /*.on_shutdown_complete =*/ on_shutdown_complete,
            /*.on_ime_show =*/ on_ime_show,
            /*.on_ime_hide =*/ on_ime_hide,
            /*.get_clipboard_contents =*/ get_clipboard_contents,
            /*.set_clipboard_contents =*/ set_clipboard_contents,
            /*.on_media_session_metadata =*/ on_media_session_metadata,
            /*.on_media_session_playback_state_change =*/ on_media_session_playback_state_change,
            /*.on_media_session_set_position_state =*/ on_media_session_set_position_state,
            /*.prompt_alert =*/ prompt_alert,
            /*.prompt_ok_cancel =*/ prompt_ok_cancel,
            /*.prompt_yes_no =*/ prompt_yes_no,
            /*.prompt_input =*/ prompt_input,
            /*.on_devtools_started =*/ on_devtools_started,
            /*.show_context_menu =*/ show_context_menu,
            /*.on_log_output =*/ on_log_output
        };
        
        this->initRenderer(cio, wakeup, chc);
    }

    // Updates first.
    bool update;
    {
        std::lock_guard<std::mutex> lock(m_updateLock);
        if (m_updateOnce || m_updateContinuously) {
            update = true;
            m_updateOnce = false;
        } else {
            update = false;
        }
    }
    if (update) perform_updates();

    // Service task queue.
    while (true) {
        std::function<void()> task;
        {
            std::lock_guard<std::mutex> lock(m_servoTasksLock);
            if (m_servoTasks.empty()) {
                break;
            } else {
                task = m_servoTasks.front();
                m_servoTasks.pop_front();
            }
        }
        task();
    }
}

void ServoUnityWindow::cleanupRenderer(void) {
    if (!s_servo) {
        SERVOUNITYLOGw("Cleanup renderer called with no renderer active.\n");
        return;
    }
    SERVOUNITYLOGd("Cleaning up renderer...\n");

    // First, clear waiting tasks and ensure no new tasks are queued while shutting down.
    std::lock_guard<std::mutex> tasksLock(m_servoTasksLock);
    m_servoTasks.clear();

    // Next, we'll request shutdown and wait on callback on_shutdown_complete before
    // finishing with deinit().
    m_waitingForShutdown = true;
    utilTime timeStart = getTimeNow();
    request_shutdown();
    while (m_waitingForShutdown == true) {
        if (millisecondsElapsedSince(timeStart) > 2000L) {
            SERVOUNITYLOGw("Timed out waiting for Servo shutdown.\n");
            break;
        }
        perform_updates();
    }

    deinit();
    s_servo = nullptr;

    queueBrowserEventCallbackTask(uidExt(), ServoUnityBrowserEvent_Shutdown, 0, 0);
    SERVOUNITYLOGd("Cleaning up renderer... DONE.\n");
}

void ServoUnityWindow::runOnServoThread(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(m_servoTasksLock);
    m_servoTasks.push_back(task);
}

void ServoUnityWindow::queueBrowserEventCallbackTask(int uidExt, int eventType, int eventData1, int eventData2) {
    std::lock_guard<std::mutex> lock(m_browserEventCallbackTasksLock);
    BROWSEREVENTCALLBACKTASK task = {uidExt, eventType, eventData1, eventData2};
    m_browserEventCallbackTasks.push_back(task);
}

void ServoUnityWindow::serviceWindowEvents() {
    // Service task queue.
    while (true) {
        BROWSEREVENTCALLBACKTASK task;
        {
            std::lock_guard<std::mutex> lock(m_browserEventCallbackTasksLock);
            if (m_browserEventCallbackTasks.empty()) {
                break;
            } else {
                task = m_browserEventCallbackTasks.front();
                m_browserEventCallbackTasks.pop_front();
            }
        }
        if (m_browserEventCallback) (*m_browserEventCallback)(task.uidExt, task.eventType, task.eventData1, task.eventData2);
    }
}

std::string ServoUnityWindow::windowTitle(void)
{
    return m_title;
}

std::string ServoUnityWindow::windowURL(void)
{
    return m_URL;
}

void ServoUnityWindow::pointerEnter() {
	SERVOUNITYLOGd("ServoUnityWindow::pointerEnter()\n");
}

void ServoUnityWindow::pointerExit() {
	SERVOUNITYLOGd("ServoUnityWindow::pointerExit()\n");
}

void ServoUnityWindow::pointerOver(int x, int y) {
	SERVOUNITYLOGd("ServoUnityWindow::pointerOver(%d, %d)\n", x, y);
    if (!s_servo) return;

    runOnServoThread([=] {mouse_move((float)x, (float)y);});
}

static CMouseButton getServoButton(int button) {
    switch (button) {
        case ServoUnityPointerEventMouseButtonID_Left:
            return CMouseButton::Left;
            break;
        case ServoUnityPointerEventMouseButtonID_Right:
            return CMouseButton::Right;
            break;
        case ServoUnityPointerEventMouseButtonID_Middle:
            return CMouseButton::Middle;
            break;
        default:
            SERVOUNITYLOGw("getServoButton unknown button %d.\n", button);
            return CMouseButton::Left;
            break;
    };
}

void ServoUnityWindow::pointerPress(int button, int x, int y) {
	SERVOUNITYLOGd("ServoUnityWindow::pointerPress(%d, %d, %d)\n", button, x, y);
    if (!s_servo) return;
    runOnServoThread([=] {mouse_down((float)x, (float)y, getServoButton(button));});
}

void ServoUnityWindow::pointerRelease(int button, int x, int y) {
	SERVOUNITYLOGd("ServoUnityWindow::pointerRelease(%d, %d, %d)\n", button, x, y);
    if (!s_servo) return;
    runOnServoThread([=] {mouse_up((float)x, (float)y, getServoButton(button));});
}

void ServoUnityWindow::pointerClick(int button, int x, int y) {
    SERVOUNITYLOGd("ServoUnityWindow::pointerClick(%d, %d, %d)\n", button, x, y);
    if (!s_servo) return;
    if (button != 0) return; // Servo assumes that "clicks" arise only from the primary button.
    runOnServoThread([=] {click((float)x, (float)y);});
}

void ServoUnityWindow::pointerScrollDiscrete(int x_scroll, int y_scroll, int x, int y) {
	SERVOUNITYLOGd("ServoUnityWindow::pointerScrollDiscrete(%d, %d, %d, %d)\n", x_scroll, y_scroll, x, y);
    if (!s_servo) return;
    runOnServoThread([=] {scroll(x_scroll, y_scroll, x, y);});
}

void ServoUnityWindow::keyEvent(int upDown, int keyCode, int character) {
	SERVOUNITYLOGd("ServoUnityWindow::keyEvent(%d, %d, %d)\n", upDown, keyCode, character);
    if (!s_servo) return;
    int kc = character;
    CKeyType kt;
    switch (keyCode) {
        //case ServoUnityKeyCode_Null: kt = CKeyType::kNull; break;
        case ServoUnityKeyCode_Character: kt = CKeyType::kCharacter; break;
        case ServoUnityKeyCode_Backspace: kt = CKeyType::kBackspace; break;
        case ServoUnityKeyCode_Delete: kt = CKeyType::kDelete; break;
        case ServoUnityKeyCode_Tab: kt = CKeyType::kTab; break;
        //case ServoUnityKeyCode_Clear: kt = CKeyType::kClear; break;
        case ServoUnityKeyCode_Return: kt = CKeyType::kEnter; break;
        case ServoUnityKeyCode_Pause: kt = CKeyType::kPause; break;
        case ServoUnityKeyCode_Escape: kt = CKeyType::kEscape; break;
        case ServoUnityKeyCode_Space: kt = CKeyType::kCharacter; kc = ' '; break;
        case ServoUnityKeyCode_UpArrow: kt = CKeyType::kUpArrow; break;
        case ServoUnityKeyCode_DownArrow: kt = CKeyType::kDownArrow; break;
        case ServoUnityKeyCode_RightArrow: kt = CKeyType::kRightArrow; break;
        case ServoUnityKeyCode_LeftArrow: kt = CKeyType::kLeftArrow; break;
        case ServoUnityKeyCode_Insert: kt = CKeyType::kInsert; break;
        case ServoUnityKeyCode_Home: kt = CKeyType::kHome; break;
        case ServoUnityKeyCode_End: kt = CKeyType::kEnd; break;
        case ServoUnityKeyCode_PageUp: kt = CKeyType::kPageUp; break;
        case ServoUnityKeyCode_PageDown: kt = CKeyType::kPageDown; break;
        case ServoUnityKeyCode_F1: kt = CKeyType::kF1; break;
        case ServoUnityKeyCode_F2: kt = CKeyType::kF2; break;
        case ServoUnityKeyCode_F3: kt = CKeyType::kF3; break;
        case ServoUnityKeyCode_F4: kt = CKeyType::kF4; break;
        case ServoUnityKeyCode_F5: kt = CKeyType::kF5; break;
        case ServoUnityKeyCode_F6: kt = CKeyType::kF6; break;
        case ServoUnityKeyCode_F7: kt = CKeyType::kF7; break;
        case ServoUnityKeyCode_F8: kt = CKeyType::kF8; break;
        case ServoUnityKeyCode_F9: kt = CKeyType::kF9; break;
        case ServoUnityKeyCode_F10: kt = CKeyType::kF10; break;
        case ServoUnityKeyCode_F11: kt = CKeyType::kF11; break;
        case ServoUnityKeyCode_F12: kt = CKeyType::kF12; break;
        //case ServoUnityKeyCode_F13: kt = CKeyType::kF13; break;
        //case ServoUnityKeyCode_F14: kt = CKeyType::kF14; break;
        //case ServoUnityKeyCode_F15: kt = CKeyType::kF15; break;
        //case ServoUnityKeyCode_F16: kt = CKeyType::kF16; break;
        //case ServoUnityKeyCode_F17: kt = CKeyType::kF17; break;
        //case ServoUnityKeyCode_F18: kt = CKeyType::kF18; break;
        //case ServoUnityKeyCode_F19: kt = CKeyType::kF19; break;
        case ServoUnityKeyCode_Numlock: kt = CKeyType::kNumLock; break;
        case ServoUnityKeyCode_CapsLock: kt = CKeyType::kCapsLock; break;
        case ServoUnityKeyCode_ScrollLock: kt = CKeyType::kScrollLock; break;
        case ServoUnityKeyCode_RightShift: kt = CKeyType::kShift; break;
        case ServoUnityKeyCode_LeftShift: kt = CKeyType::kShift; break;
        case ServoUnityKeyCode_RightControl: kt = CKeyType::kControl; break;
        case ServoUnityKeyCode_LeftControl: kt = CKeyType::kControl; break;
        case ServoUnityKeyCode_RightAlt: kt = CKeyType::kOptionAlt; break;
        case ServoUnityKeyCode_LeftAlt: kt = CKeyType::kOptionAlt; break;
        case ServoUnityKeyCode_LeftCommand: kt = CKeyType::kCommandWindows; break;
        case ServoUnityKeyCode_LeftWindows: kt = CKeyType::kCommandWindows; break;
        case ServoUnityKeyCode_RightCommand: kt = CKeyType::kCommandWindows; break;
        case ServoUnityKeyCode_RightWindows: kt = CKeyType::kCommandWindows; break;
        case ServoUnityKeyCode_AltGr: kt = CKeyType::kAltGr; break;
        case ServoUnityKeyCode_Help: kt = CKeyType::kHelp; break;
        case ServoUnityKeyCode_Print: kt = CKeyType::kPrint; break;
        //case ServoUnityKeyCode_SysReq: kt = CKeyType::kSysReq; break;
        //case ServoUnityKeyCode_Break: kt = CKeyType::kBreak; break;
        //case ServoUnityKeyCode_Menu: kt = CKeyType::kMenu; break;
        case ServoUnityKeyCode_Keypad0: kt = CKeyType::kCharacter; kc = '0'; break;
        case ServoUnityKeyCode_Keypad1: kt = CKeyType::kCharacter; kc = '1'; break;
        case ServoUnityKeyCode_Keypad2: kt = CKeyType::kCharacter; kc = '2'; break;
        case ServoUnityKeyCode_Keypad3: kt = CKeyType::kCharacter; kc = '3'; break;
        case ServoUnityKeyCode_Keypad4: kt = CKeyType::kCharacter; kc = '4'; break;
        case ServoUnityKeyCode_Keypad5: kt = CKeyType::kCharacter; kc = '5'; break;
        case ServoUnityKeyCode_Keypad6: kt = CKeyType::kCharacter; kc = '6'; break;
        case ServoUnityKeyCode_Keypad7: kt = CKeyType::kCharacter; kc = '7'; break;
        case ServoUnityKeyCode_Keypad8: kt = CKeyType::kCharacter; kc = '8'; break;
        case ServoUnityKeyCode_Keypad9: kt = CKeyType::kCharacter; kc = '9'; break;
        case ServoUnityKeyCode_KeypadPeriod: kt = CKeyType::kCharacter; kc = '.'; break;
        case ServoUnityKeyCode_KeypadDivide: kt = CKeyType::kCharacter; kc = '/'; break;
        case ServoUnityKeyCode_KeypadMultiply: kt = CKeyType::kCharacter; kc = '*'; break;
        case ServoUnityKeyCode_KeypadMinus: kt = CKeyType::kCharacter; kc = '-'; break;
        case ServoUnityKeyCode_KeypadPlus: kt = CKeyType::kCharacter; kc = '+'; break;
        case ServoUnityKeyCode_KeypadEnter: kt = CKeyType::kEnter; break;
        case ServoUnityKeyCode_KeypadEquals: kt = CKeyType::kCharacter; kc = '='; break;
        default: return;
    }

    if (upDown == 1) runOnServoThread([=] {key_down(kc, kt);});
    else runOnServoThread([=] {key_up(kc, kt);});
}

void ServoUnityWindow::refresh()
{
    if (!s_servo) return;
    runOnServoThread([=] {::refresh();});
}

void ServoUnityWindow::reload()
{
    if (!s_servo) return;
    runOnServoThread([=] {::reload();});
}

void ServoUnityWindow::stop()
{
    if (!s_servo) return;
    runOnServoThread([=] {::stop();});
}

void ServoUnityWindow::goBack()
{
    if (!s_servo) return;
    runOnServoThread([=] {go_back();});
}

void ServoUnityWindow::goForward()
{
    if (!s_servo) return;
    runOnServoThread([=] {go_forward();});
}

void ServoUnityWindow::goHome()
{
    if (!s_servo) return;
    // TODO: fetch the homepage from prefs.
    runOnServoThread([=] {
        if (is_uri_valid(s_param_Homepage.c_str())) {
            load_uri(s_param_Homepage.c_str());
        };
    });
}

void ServoUnityWindow::navigate(const std::string& urlOrSearchString)
{
    if (!s_servo) return;
    runOnServoThread([=] {
        if (is_uri_valid(urlOrSearchString.c_str())) {
            load_uri(urlOrSearchString.c_str());
        } else {
            std::string uri;
            // It's not a valid URI, but might be a domain name without method.
            // Look for bare minimum of a '.'' before any '/'.
            size_t dotPos = urlOrSearchString.find('.');
            size_t slashPos = urlOrSearchString.find('/');
            if (dotPos != std::string::npos && (slashPos == std::string::npos || slashPos > dotPos)) {
                std::string withMethod = std::string("https://" + urlOrSearchString);
                if (is_uri_valid(withMethod.c_str())) {
                    uri = withMethod;
                } else {
                    uri = s_param_SearchURI + urlOrSearchString;
                }
            } else {
                uri = s_param_SearchURI + urlOrSearchString;
            }
            if (is_uri_valid(uri.c_str())) {
                load_uri(uri.c_str());
            } else {
                SERVOUNITYLOGe("Malformed search string.\n");
            }
        }
    });
}

//
// Callback implementations. These are all necesarily static, so have to fetch the active instance
// via the static instance pointer s_servo.
//
// Callbacks can come from any Servo thread (and there are many) so care must be taken
// to ensure that any call back into Unity is on the Unity thread, or any work done
// in Servo is routed back to the main Servo thread.
//

void ServoUnityWindow::on_load_started(void)
{
    SERVOUNITYLOGd("servo callback on_load_started\n");
    if (!s_servo) return;
    s_servo->queueBrowserEventCallbackTask(s_servo->uidExt(), ServoUnityBrowserEvent_LoadStateChanged, 1, 0);
}

void ServoUnityWindow::on_load_ended(void)
{
    SERVOUNITYLOGd("servo callback on_load_ended\n");
    if (!s_servo) return;
    s_servo->queueBrowserEventCallbackTask(s_servo->uidExt(), ServoUnityBrowserEvent_LoadStateChanged, 0, 0);
}

void ServoUnityWindow::on_title_changed(const char *title)
{
    SERVOUNITYLOGd("servo callback on_title_changed: %s\n", title);
    if (!s_servo) return;
    s_servo->m_title = std::string(title);
    s_servo->queueBrowserEventCallbackTask(s_servo->uidExt(), ServoUnityBrowserEvent_TitleChanged, 0, 0);
}

bool ServoUnityWindow::on_allow_navigation(const char *url)
{
    SERVOUNITYLOGd("servo callback on_allow_navigation: %s\n", url);
    return true;
}

void ServoUnityWindow::on_url_changed(const char *url)
{
    SERVOUNITYLOGd("servo callback on_url_changed: %s\n", url);
    if (!s_servo) return;
    s_servo->m_URL = std::string(url);
    s_servo->queueBrowserEventCallbackTask(s_servo->uidExt(), ServoUnityBrowserEvent_URLChanged, 0, 0);
}

void ServoUnityWindow::on_history_changed(bool can_go_back, bool can_go_forward)
{
    SERVOUNITYLOGd("servo callback on_history_changed: can_go_back:%s, can_go_forward:%s\n", can_go_back ? "true" : "false", can_go_forward ? "true" : "false");
    if (!s_servo) return;
    s_servo->queueBrowserEventCallbackTask(s_servo->uidExt(), ServoUnityBrowserEvent_HistoryChanged, can_go_back ? 1 : 0, can_go_forward ? 1 : 0);
}

void ServoUnityWindow::on_animating_changed(bool animating)
{
    SERVOUNITYLOGd("servo callback on_animating_changed(%s)\n", animating ? "true" : "false");
    if (!s_servo) return;
    std::lock_guard<std::mutex> lock(s_servo->m_updateLock);
    s_servo->m_updateContinuously = animating;
}

void ServoUnityWindow::on_shutdown_complete(void)
{
    SERVOUNITYLOGd("servo callback on_shutdown_complete\n");
    if (!s_servo) return;
    s_servo->m_waitingForShutdown = false;
}

void ServoUnityWindow::on_ime_show(const char *text, int32_t x, int32_t y, int32_t width, int32_t height)
{
    SERVOUNITYLOGd("servo callback on_ime_show(text:%s, x:%d, y:%d, width:%d, height:%d)\n");
    if (!s_servo) return;
    s_servo->queueBrowserEventCallbackTask(s_servo->uidExt(), ServoUnityBrowserEvent_IMEStateChanged, 1, 0);
}

void ServoUnityWindow::on_ime_hide(void)
{
    SERVOUNITYLOGi("servo callback on_ime_hide\n");
    if (!s_servo) return;
    s_servo->queueBrowserEventCallbackTask(s_servo->uidExt(), ServoUnityBrowserEvent_IMEStateChanged, 0, 0);
}

const char *ServoUnityWindow::get_clipboard_contents(void)
{
    SERVOUNITYLOGi("servo callback get_clipboard_contents\n");
    SERVOUNITYLOGw("UNIMPLEMENTED\n");
    return nullptr;
}

void ServoUnityWindow::set_clipboard_contents(const char *contents)
{
    SERVOUNITYLOGi("servo callback set_clipboard_contents: %s\n", contents);
    SERVOUNITYLOGw("UNIMPLEMENTED\n");
}

void ServoUnityWindow::on_media_session_metadata(const char *title, const char *album, const char *artist)
{
    SERVOUNITYLOGi("servo callback on_media_session_metadata: title:%s, album:%s, artist:%s\n", title, album, artist);
    SERVOUNITYLOGw("UNIMPLEMENTED\n");
}

void ServoUnityWindow::on_media_session_playback_state_change(CMediaSessionPlaybackState state)
{
    const char *stateA;
    switch (state) {
        case CMediaSessionPlaybackState::None:
            stateA = "None";
            break;
        case CMediaSessionPlaybackState::Paused:
            stateA = "Paused";
            break;
        case CMediaSessionPlaybackState::Playing:
            stateA = "Playing";
            break;
        default:
            stateA = "";
            break;
    }
    SERVOUNITYLOGi("servo callback on_media_session_playback_state_change: %s\n", stateA);
    SERVOUNITYLOGw("UNIMPLEMENTED\n");
}

void ServoUnityWindow::on_media_session_set_position_state(double duration, double position, double playback_rate)
{
    SERVOUNITYLOGi("servo callback on_media_session_set_position_state: duration:%f, position:%f, playback_rate:%f\n", duration, position, playback_rate);
    SERVOUNITYLOGw("UNIMPLEMENTED\n");
}

void ServoUnityWindow::prompt_alert(const char *message, bool trusted)
{
    SERVOUNITYLOGi("servo callback prompt_alert%s: %s\n", trusted ? " (trusted)" : "", message);
    SERVOUNITYLOGw("UNIMPLEMENTED\n");
}

CPromptResult ServoUnityWindow::prompt_ok_cancel(const char *message, bool trusted)
{
    SERVOUNITYLOGi("servo callback prompt_ok_cancel%s: %s\n", trusted ? " (trusted)" : "", message);
    SERVOUNITYLOGw("UNIMPLEMENTED\n");
    return CPromptResult::Dismissed;
}

CPromptResult ServoUnityWindow::prompt_yes_no(const char *message, bool trusted)
{
    SERVOUNITYLOGi("servo callback prompt_yes_no%s: %s\n", trusted ? " (trusted)" : "", message);
    SERVOUNITYLOGw("UNIMPLEMENTED\n");
    return CPromptResult::Dismissed;
}

const char *ServoUnityWindow::prompt_input(const char *message, const char *def, bool trusted)
{
    SERVOUNITYLOGi("servo callback prompt_input%s: %s\n", trusted ? " (trusted)" : "", message);
    SERVOUNITYLOGw("UNIMPLEMENTED\n");
    return def;
}

void ServoUnityWindow::on_devtools_started(CDevtoolsServerState result, unsigned int port, const char *token)
{
    const char *resultA;
    switch (result) {
        case CDevtoolsServerState::Error:
            resultA = "Error";
            break;
        case CDevtoolsServerState::Started:
            resultA = "Started";
            break;
        default:
            resultA = "";
            break;
    }
    SERVOUNITYLOGi("servo callback on_devtools_started: result:%s, port:%d\n", resultA, port);
    SERVOUNITYLOGw("UNIMPLEMENTED\n");
}

void ServoUnityWindow::show_context_menu(const char *title, const char *const *items_list, uint32_t items_size)
{
    SERVOUNITYLOGi("servo callback show_context_menu: title:%s\n", title);
    for (int i = 0; i < (int)items_size; i++) {
        SERVOUNITYLOGi("    item %n:%s\n", i, items_list[i]);
    }
    SERVOUNITYLOGw("UNIMPLEMENTED\n");
    on_context_menu_closed(CContextMenuResult::Dismissed_, 0);
}

void ServoUnityWindow::on_log_output(const char *buffer, uint32_t buffer_length)
{
    SERVOUNITYLOGi("servo callback on_log_output: %s\n", buffer);
}

void ServoUnityWindow::wakeup(void)
{
    SERVOUNITYLOGd("servo callback wakeup on thread %" PRIu64 "\n", getThreadID());
    if (!s_servo) return;
    std::lock_guard<std::mutex> lock(s_servo->m_updateLock);
    s_servo->m_updateOnce = true;
}

