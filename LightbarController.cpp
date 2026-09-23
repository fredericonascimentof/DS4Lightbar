#include "LightbarController.h"

#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QTime>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
extern "C" {
#include <hidsdi.h>
}
#include <setupapi.h>
#endif

namespace {
constexpr int CycleSteps = 180;
constexpr float CycleMinBrightness = 0.14f;
constexpr float Pi = 3.14159265358979323846f;

float smoothstep(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

QString sdlText(const char* value)
{
    return value && value[0] != '\0' ? QString::fromUtf8(value) : "Unknown";
}

QString hex16(Uint16 value)
{
    return QString("0x%1").arg(value, 4, 16, QLatin1Char('0')).toUpper();
}

bool looksLikeDualShock(const QString& name, Uint16 vendor, Uint16 product)
{
    const QString lowerName = name.toLower();

    return vendor == 0x054c ||
        lowerName.contains("wireless controller") ||
        lowerName.contains("dualshock") ||
        lowerName.contains("dual shock") ||
        lowerName.contains("ps4") ||
        lowerName.contains("sony") ||
        product == 0x05c4 ||
        product == 0x09cc;
}

bool isDualShockProduct(Uint16 vendor, Uint16 product)
{
    return vendor == 0x054c &&
        (product == 0x05c4 ||
         product == 0x09cc ||
         product == 0x0ba0);
}

QString wideText(const wchar_t* value)
{
    return value && value[0] != L'\0' ? QString::fromWCharArray(value) : "Unknown";
}

QString hidBusName(SDL_hid_bus_type bus)
{
    switch (bus) {
    case SDL_HID_API_BUS_USB:
        return "USB";
    case SDL_HID_API_BUS_BLUETOOTH:
        return "Bluetooth";
    default:
        return "Unknown";
    }
}

#ifdef Q_OS_WIN
QString windowsErrorText(DWORD error)
{
    wchar_t* message = nullptr;
    const DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        0,
        reinterpret_cast<LPWSTR>(&message),
        0,
        nullptr
    );

    if (size == 0 || !message) {
        return QString("Windows error %1").arg(error);
    }

    QString text = QString::fromWCharArray(message).trimmed();
    LocalFree(message);
    return text;
}

bool windowsPathLooksLikeDualShock(const QString& path)
{
    const QString lower = path.toLower();
    const bool sony = lower.contains("054c");
    const bool ds4 = lower.contains("05c4") || lower.contains("09cc") || lower.contains("0ba0");
    return sony && ds4;
}

HANDLE toWindowsHandle(void* handle)
{
    return static_cast<HANDLE>(handle);
}
#endif

QString runtimeLogPath()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    if (directory.isEmpty()) {
        directory = QCoreApplication::applicationDirPath();
    }

    QDir().mkpath(directory);
    return directory + "/runtime.log";
}
}

QString LedColor::hex() const
{
    return QColor(r, g, b).name(QColor::HexRgb);
}

LightbarController::LightbarController(QObject* parent)
    : QObject(parent),
      m_modeNames({
          "Static color",
          "Spectrum cycle",
          "Lightbar off"
      })
{
    setupColors();
    initializeSdl();

    connect(&m_deviceTimer, &QTimer::timeout, this, &LightbarController::pollController);
    connect(&m_animationTimer, &QTimer::timeout, this, &LightbarController::animateLightbar);

    m_deviceTimer.setInterval(1000);
    m_animationTimer.setTimerType(Qt::PreciseTimer);
    m_animationTimer.setInterval(m_speedMs);

    m_deviceTimer.start();
    refreshAnimationTimer();

    pollController();
    applyStaticColorNow();
    appendLog("[app] DS4 Lightbar initialized.");
}

LightbarController::~LightbarController()
{
    shutdown();
}

bool LightbarController::controllerConnected() const
{
    return m_controllerConnected;
}

QString LightbarController::controllerName() const
{
    return m_controllerName;
}

QString LightbarController::connectionType() const
{
    return m_connectionType;
}

int LightbarController::batteryPercent() const
{
    return m_batteryPercent;
}

bool LightbarController::batteryCharging() const
{
    return m_batteryCharging;
}

int LightbarController::intensity() const
{
    return m_intensity;
}

void LightbarController::setIntensity(int value)
{
    const int clamped = std::clamp(value, 0, 100);

    if (m_intensity == clamped) {
        return;
    }

    m_intensity = clamped;
    
    if (staticNoPulseActive()) {
        applyStaticColorNow();
    }

    appendLog(QString("[settings] LED intensity set to %1%.").arg(m_intensity));
    emit intensityChanged();
}

int LightbarController::speedMs() const
{
    return m_speedMs;
}

void LightbarController::setSpeedMs(int value)
{
    const int clamped = std::clamp(value, 5, 250);

    if (m_speedMs == clamped) {
        return;
    }

    m_speedMs = clamped;
    m_animationTimer.setInterval(m_speedMs);
    appendLog(QString("[settings] Animation tick set to %1 ms.").arg(m_speedMs));
    emit speedMsChanged();
}

int LightbarController::modeIndex() const
{
    return m_modeIndex;
}

void LightbarController::setModeIndex(int index)
{
    const int maxModeIndex = static_cast<int>(m_modeNames.size()) - 1;
    const int clamped = std::clamp(index, 0, maxModeIndex);

    if (m_modeIndex == clamped) {
        return;
    }

    m_modeIndex = clamped;
    resetAnimation();
    refreshAnimationTimer();

    if (staticNoPulseActive()) {
        applyStaticColorNow();
    }

    if (lightbarOffActive()) {
        applyLightbarOffNow();
    }

    appendLog(QString("[settings] Light mode changed to %1.").arg(modeName()));
    emit modeChanged();
}

QString LightbarController::modeName() const
{
    return m_modeNames.value(m_modeIndex, "Unknown");
}

QStringList LightbarController::modeNames() const
{
    return m_modeNames;
}

int LightbarController::selectedColorIndex() const
{
    return m_selectedColorIndex;
}

void LightbarController::setSelectedColorIndex(int index)
{
    if (m_colors.empty()) {
        return;
    }

    const int clamped = std::clamp(index, 0, static_cast<int>(m_colors.size()) - 1);

    if (m_selectedColorIndex == clamped) {
        return;
    }

    m_selectedColorIndex = clamped;
    const LedColor color = m_colors[static_cast<size_t>(m_selectedColorIndex)];
    m_staticColor = QColor(color.r, color.g, color.b);
    resetAnimation();

    if (staticNoPulseActive()) {
        applyStaticColorNow();
    }

    appendLog(QString("[settings] Selected color changed to %1.").arg(colorNameAt(m_selectedColorIndex)));
    emit configChanged();
}

QColor LightbarController::staticColor() const
{
    return m_staticColor;
}

void LightbarController::setStaticColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }

    const QColor nextColor(color.red(), color.green(), color.blue());

    if (m_staticColor == nextColor) {
        return;
    }

    m_staticColor = nextColor;

    if (m_selectedColorIndex >= 0 && m_selectedColorIndex < static_cast<int>(m_colors.size())) {
        LedColor& selected = m_colors[static_cast<size_t>(m_selectedColorIndex)];
        selected.r = static_cast<Uint8>(m_staticColor.red());
        selected.g = static_cast<Uint8>(m_staticColor.green());
        selected.b = static_cast<Uint8>(m_staticColor.blue());
        selected.name = m_staticColor.name(QColor::HexRgb);
        emit paletteChanged();
    }

    resetAnimation();
    
    if (staticNoPulseActive()) {
        applyStaticColorNow();
    }

    appendLog(QString("[settings] Static color changed to %1.").arg(m_staticColor.name(QColor::HexRgb)));
    emit configChanged();
}

bool LightbarController::staticNoPulse() const
{
    return m_staticNoPulse;
}

void LightbarController::setStaticNoPulse(bool value)
{
    if (m_staticNoPulse == value) {
        return;
    }

    m_staticNoPulse = value;
    resetAnimation();
    refreshAnimationTimer();

    if (staticNoPulseActive()) {
        applyStaticColorNow();
    }

    appendLog(m_staticNoPulse
        ? "[settings] Static mode pulse disabled."
        : "[settings] Static mode pulse enabled.");
    emit configChanged();
}

QColor LightbarController::currentColor() const
{
    return m_currentColor;
}

QVariantList LightbarController::palette() const
{
    QVariantList items;

    for (const LedColor& color : m_colors) {
        QVariantMap item;
        item.insert("name", color.name);
        item.insert("hex", color.hex());
        items.append(item);
    }

    return items;
}

QStringList LightbarController::logs() const
{
    return m_logs;
}

bool LightbarController::logWindowVisible() const
{
    return m_logWindowVisible;
}

void LightbarController::setLogWindowVisible(bool visible)
{
    if (m_logWindowVisible == visible) {
        return;
    }

    m_logWindowVisible = visible;
    appendLog(visible ? "[log] Log window opened." : "[log] Log window hidden.");
    emit logWindowVisibleChanged();
}

void LightbarController::showLogWindow()
{
    setLogWindowVisible(true);
}

void LightbarController::hideLogWindow()
{
    setLogWindowVisible(false);
}

void LightbarController::retryController()
{
    appendLog("[controller] Manual reconnect requested.");
    closeController({});
    pollController();
}

QString LightbarController::colorNameAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_colors.size())) {
        return "Unknown";
    }

    return m_colors[static_cast<size_t>(index)].name;
}

void LightbarController::shutdown()
{
    m_animationTimer.stop();
    m_deviceTimer.stop();

    if (m_controller) {
        SDL_SetGamepadLED(m_controller, 0, 0, 0);
        SDL_CloseGamepad(m_controller);
        m_controller = nullptr;
    }

    if (m_joystick) {
        SDL_SetJoystickLED(m_joystick, 0, 0, 0);
        SDL_CloseJoystick(m_joystick);
        m_joystick = nullptr;
    }

    if (m_hidDevice) {
        sendHidColor(QColor(0, 0, 0));
        SDL_hid_close(m_hidDevice);
        m_hidDevice = nullptr;
    }

#ifdef Q_OS_WIN
    if (m_windowsHidHandle) {
        sendWindowsHidColor(QColor(0, 0, 0));
        CloseHandle(toWindowsHandle(m_windowsHidHandle));
        m_windowsHidHandle = nullptr;
    }
#endif

    m_usingJoystickFallback = false;
    m_usingHidFallback = false;
    m_usingWindowsHidFallback = false;
    m_hidBluetooth = false;
    m_windowsHidBluetooth = false;
    m_controllerConnected = false;

    if (m_sdlReady) {
        SDL_hid_exit();
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK);
        m_sdlReady = false;
    }
}

void LightbarController::initializeSdl()
{
    SDL_SetHint(SDL_HINT_AUTO_UPDATE_JOYSTICKS, "1");
    SDL_SetHint(SDL_HINT_HIDAPI_ENUMERATE_ONLY_CONTROLLERS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_BLACKLIST_DEVICES_EXCLUDED, "0x054c/0x05c4,0x054c/0x09cc,0x054c/0x0ba0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_DIRECTINPUT, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_GAMEINPUT, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_REPORT_INTERVAL, "4");
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "1");
    SDL_SetHint(SDL_HINT_WINDOWS_GAMEINPUT, "1");
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK)) {
        appendLog(QString("[sdl] Failed to initialize SDL input subsystem: %1").arg(SDL_GetError()));
        return;
    }

    m_sdlReady = true;
    SDL_hid_init();
    appendLog("[sdl] SDL gamepad and joystick subsystems initialized.");
}

void LightbarController::setupColors()
{
    m_colors = {
        {255, 0, 0, "red"},
        {0, 195, 255, "cyan"},
        {120, 92, 255, "violet"},
        {255, 73, 218, "magenta"},
        {0, 255, 120, "green"},
        {255, 140, 0, "orange"}
    };
}

void LightbarController::pollController()
{
    if (!m_sdlReady) {
        return;
    }

    SDL_PumpEvents();
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_GAMEPAD_ADDED || event.type == SDL_EVENT_JOYSTICK_ADDED) {
            if (!m_controllerConnected) {
                appendLog("[controller] Controller detected by SDL event. Trying to open...");
                openFirstController();
            }
        }

        if (event.type == SDL_EVENT_GAMEPAD_REMOVED || event.type == SDL_EVENT_JOYSTICK_REMOVED) {
            if (m_controllerConnected) {
                closeController("[controller] Controller removed.");
            }
        }
    }

    SDL_UpdateGamepads();
    SDL_UpdateJoysticks();

    if (m_controller && !SDL_GamepadConnected(m_controller)) {
        closeController("[controller] Controller disconnected.");
    }

    if (m_joystick && !SDL_JoystickConnected(m_joystick)) {
        closeController("[controller] Joystick fallback disconnected.");
    }

    if (!m_controller && !m_joystick) {
        openFirstController();
    }

    updateDeviceStatus();
}

bool LightbarController::openFirstController()
{
    SDL_UpdateGamepads();
    SDL_UpdateJoysticks();

    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);

    if (!gamepads || count == 0) {
        if (gamepads) {
            SDL_free(gamepads);
        }

        if (!m_reportedNoController) {
            appendLog("[controller] SDL scan found 0 gamepads.");
        }

        return openJoystickFallback();
    }

    if (!m_reportedNoController) {
        appendLog(QString("[controller] SDL scan found %1 gamepad(s).").arg(count));
    }

    int preferredIndex = 0;

    for (int i = 0; i < count; i++) {
        const QString name = sdlText(SDL_GetGamepadNameForID(gamepads[i]));
        const Uint16 vendor = SDL_GetGamepadVendorForID(gamepads[i]);
        const Uint16 product = SDL_GetGamepadProductForID(gamepads[i]);

        if (!m_reportedNoController) {
            appendLog(QString("[controller] gamepad[%1]: %2 vendor=%3 product=%4")
                .arg(i)
                .arg(name)
                .arg(hex16(vendor))
                .arg(hex16(product)));
        }

        if (looksLikeDualShock(name, vendor, product)) {
            preferredIndex = i;
            break;
        }
    }

    for (int attempt = 0; attempt < count; attempt++) {
        const int index = (attempt == 0)
            ? preferredIndex
            : attempt - 1 < preferredIndex ? attempt - 1 : attempt;

        if (index < 0 || index >= count) {
            continue;
        }

        m_controller = SDL_OpenGamepad(gamepads[index]);

        if (m_controller) {
            break;
        }

        appendLog(QString("[controller] Failed to open gamepad[%1]: %2").arg(index).arg(SDL_GetError()));
    }

    SDL_free(gamepads);

    if (!m_controller) {
        return openJoystickFallback();
    }

    m_usingJoystickFallback = false;
    m_reportedNoController = false;
    m_reportedLedFailure = false;
    m_controllerConnected = true;
    resetAnimation();
    updateDeviceStatus();
    refreshAnimationTimer();

    if (staticNoPulseActive()) {
        applyStaticColorNow();
    }

    if (lightbarOffActive()) {
        applyLightbarOffNow();
    }

    appendLog("[controller] Controller opened successfully.");
    emit controllerStatusChanged();
    return true;
}

bool LightbarController::openJoystickFallback()
{
    int count = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&count);

    if (!joysticks || count == 0) {
        if (joysticks) {
            SDL_free(joysticks);
        }

        if (!m_reportedNoController) {
            appendLog("[controller] SDL scan found 0 joysticks.");
        }

        return openHidFallback();
    }

    if (!m_reportedNoController) {
        appendLog(QString("[controller] SDL scan found %1 joystick(s).").arg(count));
    }

    int preferredIndex = 0;

    for (int i = 0; i < count; i++) {
        const QString name = sdlText(SDL_GetJoystickNameForID(joysticks[i]));
        const Uint16 vendor = SDL_GetJoystickVendorForID(joysticks[i]);
        const Uint16 product = SDL_GetJoystickProductForID(joysticks[i]);

        if (!m_reportedNoController) {
            appendLog(QString("[controller] joystick[%1]: %2 vendor=%3 product=%4")
                .arg(i)
                .arg(name)
                .arg(hex16(vendor))
                .arg(hex16(product)));
        }

        if (looksLikeDualShock(name, vendor, product)) {
            preferredIndex = i;
            break;
        }
    }

    for (int attempt = 0; attempt < count; attempt++) {
        const int index = (attempt == 0)
            ? preferredIndex
            : attempt - 1 < preferredIndex ? attempt - 1 : attempt;

        if (index < 0 || index >= count) {
            continue;
        }

        m_joystick = SDL_OpenJoystick(joysticks[index]);

        if (m_joystick) {
            break;
        }

        appendLog(QString("[controller] Failed to open joystick[%1]: %2").arg(index).arg(SDL_GetError()));
    }

    SDL_free(joysticks);

    if (!m_joystick) {
        appendLog(QString("[controller] Failed to open joystick fallback: %1").arg(SDL_GetError()));
        return openHidFallback();
    }

    m_usingJoystickFallback = true;
    m_reportedNoController = false;
    m_reportedLedFailure = false;
    m_controllerConnected = true;
    resetAnimation();
    updateDeviceStatus();
    refreshAnimationTimer();

    if (staticNoPulseActive()) {
        applyStaticColorNow();
    }

    if (lightbarOffActive()) {
        applyLightbarOffNow();
    }

    appendLog("[controller] Joystick fallback opened successfully.");
    emit controllerStatusChanged();
    return true;
}

bool LightbarController::openHidFallback()
{
    SDL_hid_device_info* devices = SDL_hid_enumerate(0x054c, 0x0000);

    if (!devices) {
        if (!m_reportedNoController) {
            appendLog("[hid] No Sony HID device found through SDL HIDAPI. Trying Windows HID fallback.");
        }

        return openWindowsHidFallback();
    }

    SDL_hid_device_info* selected = nullptr;
    int index = 0;

    for (SDL_hid_device_info* device = devices; device; device = device->next) {
        const QString product = wideText(device->product_string);

        if (!m_reportedNoController) {
            appendLog(QString("[hid] sony[%1]: %2 vendor=%3 product=%4 bus=%5 usagePage=0x%6 usage=0x%7")
                .arg(index)
                .arg(product)
                .arg(hex16(device->vendor_id))
                .arg(hex16(device->product_id))
                .arg(hidBusName(device->bus_type))
                .arg(device->usage_page, 4, 16, QLatin1Char('0'))
                .arg(device->usage, 4, 16, QLatin1Char('0')));
        }

        if (!selected && isDualShockProduct(device->vendor_id, device->product_id)) {
            selected = device;
        }

        index++;
    }

    if (!selected || !selected->path) {
        SDL_hid_free_enumeration(devices);

        if (!m_reportedNoController) {
            appendLog("[hid] Sony HID device found, but no supported DualShock 4 path was available. Trying Windows HID fallback.");
        }

        return openWindowsHidFallback();
    }

    m_hidDevice = SDL_hid_open_path(selected->path);

    if (!m_hidDevice) {
        const QString error = SDL_GetError();
        SDL_hid_free_enumeration(devices);
        appendLog(QString("[hid] Failed to open DualShock 4 HID path: %1").arg(error));
        return openWindowsHidFallback();
    }

    SDL_hid_set_nonblocking(m_hidDevice, 1);

    m_hidBluetooth = selected->bus_type == SDL_HID_API_BUS_BLUETOOTH;
    m_usingHidFallback = true;
    m_usingJoystickFallback = false;
    m_reportedNoController = false;
    m_reportedLedFailure = false;
    m_controllerConnected = true;
    m_controllerName = wideText(selected->product_string);
    m_connectionType = m_hidBluetooth ? "Wireless (HID)" : "USB (HID)";
    m_batteryPercent = -1;
    m_batteryCharging = false;

    SDL_hid_free_enumeration(devices);

    resetAnimation();
    refreshAnimationTimer();

    if (staticNoPulseActive()) {
        applyStaticColorNow();
    }

    if (lightbarOffActive()) {
        applyLightbarOffNow();
    }

    appendLog(QString("[hid] DualShock 4 opened through direct HID fallback over %1.").arg(m_connectionType));
    emit controllerStatusChanged();
    return true;
}

bool LightbarController::openWindowsHidFallback()
{
#ifdef Q_OS_WIN
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO deviceInfo = SetupDiGetClassDevsW(
        &hidGuid,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (deviceInfo == INVALID_HANDLE_VALUE) {
        appendLog(QString("[winhid] Failed to enumerate HID interfaces: %1").arg(windowsErrorText(GetLastError())));
        return false;
    }

    bool sawDualShockPath = false;

    for (DWORD index = 0;; index++) {
        SP_DEVICE_INTERFACE_DATA interfaceData {};
        interfaceData.cbSize = sizeof(interfaceData);

        if (!SetupDiEnumDeviceInterfaces(deviceInfo, nullptr, &hidGuid, index, &interfaceData)) {
            const DWORD error = GetLastError();

            if (error != ERROR_NO_MORE_ITEMS) {
                appendLog(QString("[winhid] HID interface enumeration stopped: %1").arg(windowsErrorText(error)));
            }

            break;
        }

        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(deviceInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);

        if (requiredSize == 0) {
            continue;
        }

        std::vector<BYTE> detailBuffer(requiredSize);
        auto* detailData = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detailBuffer.data());
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA devInfoData {};
        devInfoData.cbSize = sizeof(devInfoData);

        if (!SetupDiGetDeviceInterfaceDetailW(
                deviceInfo,
                &interfaceData,
                detailData,
                requiredSize,
                nullptr,
                &devInfoData
            )) {
            continue;
        }

        const QString path = QString::fromWCharArray(detailData->DevicePath);

        if (!windowsPathLooksLikeDualShock(path)) {
            continue;
        }

        sawDualShockPath = true;
        appendLog(QString("[winhid] DualShock HID interface found: %1").arg(path));

        HANDLE handle = CreateFileW(
            detailData->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (handle == INVALID_HANDLE_VALUE) {
            const DWORD readWriteError = GetLastError();
            handle = CreateFileW(
                detailData->DevicePath,
                GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );

            if (handle == INVALID_HANDLE_VALUE) {
                appendLog(QString("[winhid] Failed to open HID interface: %1").arg(windowsErrorText(readWriteError)));
                continue;
            }
        }

        wchar_t productName[128] {};
        QString nextName = "Wireless Controller";

        if (HidD_GetProductString(handle, productName, sizeof(productName))) {
            nextName = QString::fromWCharArray(productName);
        }

        const QString lowerPath = path.toLower();
        m_windowsHidHandle = handle;
        m_windowsHidBluetooth = lowerPath.contains("bth") || lowerPath.contains("bluetooth");
        m_usingWindowsHidFallback = true;
        m_usingHidFallback = false;
        m_usingJoystickFallback = false;
        m_reportedNoController = false;
        m_reportedLedFailure = false;
        m_controllerConnected = true;
        m_controllerName = nextName;
        m_connectionType = m_windowsHidBluetooth ? "Wireless (Windows HID)" : "USB (Windows HID)";
        m_batteryPercent = -1;
        m_batteryCharging = false;

        SetupDiDestroyDeviceInfoList(deviceInfo);

        resetAnimation();
        refreshAnimationTimer();

        if (staticNoPulseActive()) {
            applyStaticColorNow();
        }

        if (lightbarOffActive()) {
            applyLightbarOffNow();
        }

        appendLog(QString("[winhid] DualShock 4 opened through native Windows HID over %1.").arg(m_connectionType));
        emit controllerStatusChanged();
        return true;
    }

    SetupDiDestroyDeviceInfoList(deviceInfo);

    if (!m_reportedNoController) {
        appendLog(sawDualShockPath
            ? "[winhid] DualShock 4 HID interface was found, but none could be opened."
            : "[winhid] No matching DualShock 4 HID interface found.");
        m_reportedNoController = true;
    }

    return false;
#else
    if (!m_reportedNoController) {
        appendLog("[winhid] Windows HID fallback is not available on this platform.");
        m_reportedNoController = true;
    }

    return false;
#endif
}

void LightbarController::closeController(const QString& reason)
{
    if (m_controller) {
        SDL_CloseGamepad(m_controller);
        m_controller = nullptr;
    }

    if (m_joystick) {
        SDL_CloseJoystick(m_joystick);
        m_joystick = nullptr;
    }

    if (m_hidDevice) {
        SDL_hid_close(m_hidDevice);
        m_hidDevice = nullptr;
    }

#ifdef Q_OS_WIN
    if (m_windowsHidHandle) {
        CloseHandle(toWindowsHandle(m_windowsHidHandle));
        m_windowsHidHandle = nullptr;
    }
#endif

    m_usingJoystickFallback = false;
    m_usingHidFallback = false;
    m_usingWindowsHidFallback = false;
    m_hidBluetooth = false;
    m_windowsHidBluetooth = false;
    m_reportedLedFailure = false;
    m_controllerConnected = false;
    m_controllerName = "No controller";
    m_connectionType = "Disconnected";
    m_batteryPercent = -1;
    m_batteryCharging = false;
    m_currentColor = QColor(0, 0, 0);

    if (!reason.isEmpty()) {
        appendLog(reason);
    }

    emit controllerStatusChanged();
    emit currentColorChanged();
}

void LightbarController::updateDeviceStatus()
{
    if (!m_controllerConnected || (!m_controller && !m_joystick && !m_hidDevice && !m_windowsHidHandle)) {
        return;
    }

    if (m_usingWindowsHidFallback && m_windowsHidHandle) {
        const QString nextName = m_controllerName.isEmpty() || m_controllerName == "No controller"
            ? "Wireless Controller"
            : m_controllerName;
        const QString nextConnection = m_windowsHidBluetooth ? "Wireless (Windows HID)" : "USB (Windows HID)";

        if (nextName != m_controllerName || nextConnection != m_connectionType) {
            m_controllerName = nextName;
            m_connectionType = nextConnection;
            emit controllerStatusChanged();
        }

        return;
    }

    if (m_usingHidFallback && m_hidDevice) {
        const QString nextName = m_controllerName.isEmpty() || m_controllerName == "No controller"
            ? "Wireless Controller"
            : m_controllerName;
        const QString nextConnection = m_hidBluetooth ? "Wireless (HID)" : "USB (HID)";

        if (nextName != m_controllerName || nextConnection != m_connectionType) {
            m_controllerName = nextName;
            m_connectionType = nextConnection;
            emit controllerStatusChanged();
        }

        return;
    }

    QString nextName = "Controller";
    const char* sdlName = m_controller
        ? SDL_GetGamepadName(m_controller)
        : SDL_GetJoystickName(m_joystick);

    if (sdlName && sdlName[0] != '\0') {
        nextName = QString::fromUtf8(sdlName);
    }

    QString nextConnection = "Connected";
    const SDL_JoystickConnectionState connection = m_controller
        ? SDL_GetGamepadConnectionState(m_controller)
        : SDL_GetJoystickConnectionState(m_joystick);

    if (connection == SDL_JOYSTICK_CONNECTION_WIRELESS) {
        nextConnection = "Wireless";
    } else if (connection == SDL_JOYSTICK_CONNECTION_WIRED) {
        nextConnection = "USB";
    }

    if (m_usingJoystickFallback) {
        nextConnection += " (joystick)";
    }

    int percent = -1;
    const SDL_PowerState power = m_controller
        ? SDL_GetGamepadPowerInfo(m_controller, &percent)
        : SDL_GetJoystickPowerInfo(m_joystick, &percent);
    const int nextBattery = percent >= 0 && percent <= 100 ? percent : -1;
    const bool nextCharging = power == SDL_POWERSTATE_CHARGING || power == SDL_POWERSTATE_CHARGED;

    if (nextName == m_controllerName &&
        nextConnection == m_connectionType &&
        nextBattery == m_batteryPercent &&
        nextCharging == m_batteryCharging) {
        return;
    }

    m_controllerName = nextName;
    m_connectionType = nextConnection;
    m_batteryPercent = nextBattery;
    m_batteryCharging = nextCharging;
    emit controllerStatusChanged();
}

void LightbarController::animateLightbar()
{
    if (m_colors.empty()) {
        return;
    }

    if (staticNoPulseActive()) {
        refreshAnimationTimer();
        applyStaticColorNow();
        return;
    }

    if (lightbarOffActive()) {
        refreshAnimationTimer();
        applyLightbarOffNow();
        return;
    }

    if (m_colorIndex < 0 || m_colorIndex >= static_cast<int>(m_colors.size())) {
        m_colorIndex = 0;
    }

    switch (static_cast<LightMode>(m_modeIndex)) {
    case LightMode::StaticColor: {
        if (m_step == 0) {
            appendLog(QString(m_staticNoPulse
                ? "[lightbar] Static color: %1."
                : "[lightbar] Static pulse: %1.").arg(selectedColor().name));
        }

        if (m_staticNoPulse) {
            sendColor(selectedColor());
            m_step = 1;
            break;
        }

        const float t = m_step / static_cast<float>(CycleSteps);
        const float pulse = std::sin(t * Pi);
        const float brightness = CycleMinBrightness + pulse * (1.0f - CycleMinBrightness);

        sendColor(selectedColor(), brightness);

        m_step++;

        if (m_step > CycleSteps) {
            m_step = 0;
        }

        break;
    }

    case LightMode::PulseCycle: {
        const LedColor from = m_colors[static_cast<size_t>(m_colorIndex)];
        const LedColor to = m_colors[static_cast<size_t>((m_colorIndex + 1) % static_cast<int>(m_colors.size()))];
        const float t = m_step / static_cast<float>(CycleSteps);
        const float colorT = smoothstep(t);

        if (m_step == 0) {
            appendLog(QString("[lightbar] Smooth cycle: %1 to %2.").arg(from.name, to.name));
        }

        sendColor(
            lerp(from.r, to.r, colorT),
            lerp(from.g, to.g, colorT),
            lerp(from.b, to.b, colorT)
        );

        m_step++;

        if (m_step > CycleSteps) {
            m_step = 0;
            m_colorIndex = (m_colorIndex + 1) % static_cast<int>(m_colors.size());
        }

        break;
    }

    case LightMode::LightbarOff: {
        if (m_step == 0) {
            appendLog("[lightbar] Lightbar off.");
            applyLightbarOffNow();
        }

        break;
    }
    }
}

void LightbarController::resetAnimation()
{
    m_step = 0;

    if (m_colorIndex < 0 || m_colorIndex >= static_cast<int>(m_colors.size())) {
        m_colorIndex = 0;
    }
}

bool LightbarController::staticNoPulseActive() const
{
    return static_cast<LightMode>(m_modeIndex) == LightMode::StaticColor && m_staticNoPulse;
}

bool LightbarController::lightbarOffActive() const
{
    return static_cast<LightMode>(m_modeIndex) == LightMode::LightbarOff;
}

void LightbarController::refreshAnimationTimer()
{
    if (staticNoPulseActive() || lightbarOffActive()) {
        if (m_animationTimer.isActive()) {
            m_animationTimer.stop();
        }

        return;
    }

    if (!m_animationTimer.isActive()) {
        m_animationTimer.start();
    }
}

void LightbarController::applyStaticColorNow()
{
    if (m_colors.empty() || static_cast<LightMode>(m_modeIndex) != LightMode::StaticColor) {
        return;
    }

    m_step = 1;
    sendColor(selectedColor());
}

void LightbarController::applyLightbarOffNow()
{
    m_step = 1;
    sendColor(0, 0, 0);
}

void LightbarController::appendLog(const QString& message)
{
    const QString line = QString("[%1] %2").arg(QTime::currentTime().toString("HH:mm:ss"), message);
    m_logs.append(line);
    qInfo().noquote() << line;

    QFile file(runtimeLogPath());

    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << line << '\n';
    }

    while (m_logs.size() > 200) {
        m_logs.removeFirst();
    }

    emit logsChanged();
}

LedColor LightbarController::selectedColor() const
{
    return {
        static_cast<Uint8>(std::clamp(m_staticColor.red(), 0, 255)),
        static_cast<Uint8>(std::clamp(m_staticColor.green(), 0, 255)),
        static_cast<Uint8>(std::clamp(m_staticColor.blue(), 0, 255)),
        m_selectedColorIndex >= 0 ? colorNameAt(m_selectedColorIndex) : m_staticColor.name(QColor::HexRgb)
    };
}

bool LightbarController::sendColor(const LedColor& color, float brightness)
{
    return sendColor(
        color.r,
        color.g,
        color.b,
        brightness
    );
}

bool LightbarController::sendColor(Uint8 r, Uint8 g, Uint8 b, float brightness)
{
    const QColor nextColor = scaledColor(r, g, b, brightness);

    if (m_currentColor != nextColor) {
        m_currentColor = nextColor;
        emit currentColorChanged();
    }

    if (!m_controllerConnected || (!m_controller && !m_joystick)) {
        if (m_usingWindowsHidFallback && m_windowsHidHandle) {
            return sendWindowsHidColor(nextColor);
        }

        if (m_usingHidFallback && m_hidDevice) {
            return sendHidColor(nextColor);
        }

        return true;
    }

    const bool sent = m_controller
        ? SDL_SetGamepadLED(m_controller, nextColor.red(), nextColor.green(), nextColor.blue())
        : SDL_SetJoystickLED(m_joystick, nextColor.red(), nextColor.green(), nextColor.blue());

    if (!sent) {
        if (!m_reportedLedFailure) {
            appendLog("[controller] Failed to send LED color. The controller remains connected, but this device/driver may not expose LED control.");
            m_reportedLedFailure = true;
        }

        return false;
    }

    m_reportedLedFailure = false;
    return true;
}

bool LightbarController::sendHidColor(const QColor& color)
{
    if (!m_hidDevice) {
        return false;
    }

    constexpr Uint8 effectLed = 1 << 1;

    std::array<Uint8, 78> data {};
    int reportSize = 0;
    int offset = 0;

    if (m_hidBluetooth) {
        data[0] = 0x11;
        data[1] = 0xC4;
        data[3] = effectLed;
        reportSize = 78;
        offset = 6;
    } else {
        data[0] = 0x05;
        data[1] = effectLed;
        reportSize = 32;
        offset = 4;
    }

    data[static_cast<size_t>(offset + 2)] = static_cast<Uint8>(color.red());
    data[static_cast<size_t>(offset + 3)] = static_cast<Uint8>(color.green());
    data[static_cast<size_t>(offset + 4)] = static_cast<Uint8>(color.blue());

    if (m_hidBluetooth) {
        const Uint8 hidOutputHeader = 0xA2;
        Uint32 crc = SDL_crc32(0, &hidOutputHeader, 1);
        crc = SDL_crc32(crc, data.data(), static_cast<size_t>(reportSize - 4));
        data[74] = static_cast<Uint8>(crc & 0xff);
        data[75] = static_cast<Uint8>((crc >> 8) & 0xff);
        data[76] = static_cast<Uint8>((crc >> 16) & 0xff);
        data[77] = static_cast<Uint8>((crc >> 24) & 0xff);
    }

    const int written = SDL_hid_write(m_hidDevice, data.data(), static_cast<size_t>(reportSize));

    if (written != reportSize) {
        if (!m_reportedLedFailure) {
            appendLog(QString("[hid] Failed to send lightbar color through HID fallback: %1").arg(SDL_GetError()));
            m_reportedLedFailure = true;
        }

        return false;
    }

    m_reportedLedFailure = false;
    return true;
}

bool LightbarController::sendWindowsHidColor(const QColor& color)
{
#ifdef Q_OS_WIN
    if (!m_windowsHidHandle) {
        return false;
    }

    constexpr Uint8 effectLed = 1 << 1;

    std::array<Uint8, 78> data {};
    int reportSize = 0;
    int offset = 0;

    if (m_windowsHidBluetooth) {
        data[0] = 0x11;
        data[1] = 0xC4;
        data[3] = effectLed;
        reportSize = 78;
        offset = 6;
    } else {
        data[0] = 0x05;
        data[1] = effectLed;
        reportSize = 32;
        offset = 4;
    }

    data[static_cast<size_t>(offset + 2)] = static_cast<Uint8>(color.red());
    data[static_cast<size_t>(offset + 3)] = static_cast<Uint8>(color.green());
    data[static_cast<size_t>(offset + 4)] = static_cast<Uint8>(color.blue());

    if (m_windowsHidBluetooth) {
        const Uint8 hidOutputHeader = 0xA2;
        Uint32 crc = SDL_crc32(0, &hidOutputHeader, 1);
        crc = SDL_crc32(crc, data.data(), static_cast<size_t>(reportSize - 4));
        data[74] = static_cast<Uint8>(crc & 0xff);
        data[75] = static_cast<Uint8>((crc >> 8) & 0xff);
        data[76] = static_cast<Uint8>((crc >> 16) & 0xff);
        data[77] = static_cast<Uint8>((crc >> 24) & 0xff);
    }

    HANDLE handle = toWindowsHandle(m_windowsHidHandle);
    BOOL sent = HidD_SetOutputReport(handle, data.data(), static_cast<ULONG>(reportSize));
    DWORD error = sent ? ERROR_SUCCESS : GetLastError();

    if (!sent) {
        DWORD written = 0;
        sent = WriteFile(handle, data.data(), static_cast<DWORD>(reportSize), &written, nullptr) &&
            written == static_cast<DWORD>(reportSize);

        if (!sent && error == ERROR_SUCCESS) {
            error = GetLastError();
        }
    }

    if (!sent) {
        if (!m_reportedLedFailure) {
            appendLog(QString("[winhid] Failed to send lightbar color through Windows HID: %1").arg(windowsErrorText(error)));
            m_reportedLedFailure = true;
        }

        return false;
    }

    m_reportedLedFailure = false;
    return true;
#else
    Q_UNUSED(color);
    return false;
#endif
}

QColor LightbarController::scaledColor(Uint8 r, Uint8 g, Uint8 b, float brightness) const
{
    const float intensity = std::clamp(m_intensity / 100.0f, 0.0f, 1.0f);
    const float finalBrightness = std::clamp(brightness, 0.0f, 1.0f);

    return QColor(
        static_cast<int>(std::clamp(r * intensity * finalBrightness, 0.0f, 255.0f)),
        static_cast<int>(std::clamp(g * intensity * finalBrightness, 0.0f, 255.0f)),
        static_cast<int>(std::clamp(b * intensity * finalBrightness, 0.0f, 255.0f))
    );
}

Uint8 LightbarController::lerp(Uint8 start, Uint8 end, float t)
{
    return static_cast<Uint8>(start + (end - start) * std::clamp(t, 0.0f, 1.0f));
}
