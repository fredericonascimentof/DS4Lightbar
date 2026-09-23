#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include <SDL3/SDL.h>
#include <SDL3/SDL_hidapi.h>

#include <vector>

struct LedColor {
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    QString name;

    QString hex() const;
};

class LightbarController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool controllerConnected READ controllerConnected NOTIFY controllerStatusChanged)
    Q_PROPERTY(QString controllerName READ controllerName NOTIFY controllerStatusChanged)
    Q_PROPERTY(QString connectionType READ connectionType NOTIFY controllerStatusChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY controllerStatusChanged)
    Q_PROPERTY(bool batteryCharging READ batteryCharging NOTIFY controllerStatusChanged)
    Q_PROPERTY(int intensity READ intensity WRITE setIntensity NOTIFY intensityChanged)
    Q_PROPERTY(int speedMs READ speedMs WRITE setSpeedMs NOTIFY speedMsChanged)
    Q_PROPERTY(int modeIndex READ modeIndex WRITE setModeIndex NOTIFY modeChanged)
    Q_PROPERTY(QString modeName READ modeName NOTIFY modeChanged)
    Q_PROPERTY(QStringList modeNames READ modeNames CONSTANT)
    Q_PROPERTY(int selectedColorIndex READ selectedColorIndex WRITE setSelectedColorIndex NOTIFY configChanged)
    Q_PROPERTY(QColor staticColor READ staticColor WRITE setStaticColor NOTIFY configChanged)
    Q_PROPERTY(bool staticNoPulse READ staticNoPulse WRITE setStaticNoPulse NOTIFY configChanged)
    Q_PROPERTY(QColor currentColor READ currentColor NOTIFY currentColorChanged)
    Q_PROPERTY(QVariantList palette READ palette NOTIFY paletteChanged)
    Q_PROPERTY(QStringList logs READ logs NOTIFY logsChanged)
    Q_PROPERTY(bool logWindowVisible READ logWindowVisible WRITE setLogWindowVisible NOTIFY logWindowVisibleChanged)

public:
    explicit LightbarController(QObject* parent = nullptr);
    ~LightbarController() override;

    bool controllerConnected() const;
    QString controllerName() const;
    QString connectionType() const;
    int batteryPercent() const;
    bool batteryCharging() const;

    int intensity() const;
    void setIntensity(int value);

    int speedMs() const;
    void setSpeedMs(int value);

    int modeIndex() const;
    void setModeIndex(int index);
    QString modeName() const;
    QStringList modeNames() const;

    int selectedColorIndex() const;
    void setSelectedColorIndex(int index);

    QColor staticColor() const;
    void setStaticColor(const QColor& color);

    bool staticNoPulse() const;
    void setStaticNoPulse(bool value);

    QColor currentColor() const;
    QVariantList palette() const;
    QStringList logs() const;

    bool logWindowVisible() const;
    void setLogWindowVisible(bool visible);

    Q_INVOKABLE void showLogWindow();
    Q_INVOKABLE void hideLogWindow();
    Q_INVOKABLE void retryController();
    Q_INVOKABLE QString colorNameAt(int index) const;
    Q_INVOKABLE void shutdown();

signals:
    void controllerStatusChanged();
    void intensityChanged();
    void speedMsChanged();
    void modeChanged();
    void configChanged();
    void paletteChanged();
    void currentColorChanged();
    void logsChanged();
    void logWindowVisibleChanged();

private:
    enum class LightMode {
        StaticColor = 0,
        PulseCycle = 1,
        LightbarOff = 2
    };

    void initializeSdl();
    void setupColors();
    void pollController();
    bool openFirstController();
    bool openJoystickFallback();
    bool openHidFallback();
    bool openWindowsHidFallback();
    void closeController(const QString& reason);
    void updateDeviceStatus();
    void animateLightbar();
    void resetAnimation();
    bool staticNoPulseActive() const;
    bool lightbarOffActive() const;
    void refreshAnimationTimer();
    void applyStaticColorNow();
    void applyLightbarOffNow();
    void appendLog(const QString& message);

    LedColor selectedColor() const;
    bool sendColor(const LedColor& color, float brightness = 1.0f);
    bool sendColor(Uint8 r, Uint8 g, Uint8 b, float brightness = 1.0f);
    bool sendHidColor(const QColor& color);
    bool sendWindowsHidColor(const QColor& color);
    QColor scaledColor(Uint8 r, Uint8 g, Uint8 b, float brightness = 1.0f) const;

    static Uint8 lerp(Uint8 start, Uint8 end, float t);

    SDL_Gamepad* m_controller = nullptr;
    SDL_Joystick* m_joystick = nullptr;
    SDL_hid_device* m_hidDevice = nullptr;
    void* m_windowsHidHandle = nullptr;
    bool m_usingJoystickFallback = false;
    bool m_usingHidFallback = false;
    bool m_usingWindowsHidFallback = false;
    bool m_hidBluetooth = false;
    bool m_windowsHidBluetooth = false;
    bool m_sdlReady = false;
    bool m_controllerConnected = false;
    bool m_batteryCharging = false;
    bool m_logWindowVisible = false;
    bool m_staticNoPulse = true;
    bool m_reportedNoController = false;
    bool m_reportedLedFailure = false;

    QString m_controllerName = "No controller";
    QString m_connectionType = "Disconnected";
    int m_batteryPercent = -1;
    int m_intensity = 100;
    int m_speedMs = 30;
    int m_modeIndex = static_cast<int>(LightMode::StaticColor);
    int m_selectedColorIndex = 1;
    int m_colorIndex = 0;
    int m_step = 0;

    QColor m_staticColor = QColor(0, 195, 255);
    QColor m_currentColor = QColor(0, 0, 0);
    QStringList m_logs;
    QStringList m_modeNames;
    std::vector<LedColor> m_colors;

    QTimer m_deviceTimer;
    QTimer m_animationTimer;
};
