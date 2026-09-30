#pragma once

#include <QMainWindow>

#include <array>

#include "PatliteDevice.h"
#include "PatliteTypes.h"

class QComboBox;
class QPushButton;
class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @brief 메인 윈도우.
 *
 * 위젯 배치는 Qt Designer로 편집 가능한 MainWindow.ui에서 관리하고,
 * 이 클래스는 신호 연결과 장치 제어 로직만 담당한다.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onDeviceTypeChanged(int index);
    void onConnectClicked();
    void onConnectionChanged(bool connected);
    void onApplySingle(int colorIndex);
    void onApplyTower();
    void onApplyBuzzer();
    void onStopBuzzer();
    void onReset();
    void onPoll();
    void appendLog(const QString &message);
    void appendFrame(const QByteArray &frame);

private:
    void updateEnabledState();
    void applyPreset(const patlite::TowerState &tower,
                     patlite::Buzzer buzzer, quint8 limit);
    void setActiveDevice(PatliteDevice *device);

    patlite::TowerState currentTowerSelection() const;
    static void fillLedCombo(QComboBox *box, bool includeKeep);
    static void fillBuzzerCombo(QComboBox *box);
    static void fillPitchCombo(QComboBox *box, patlite::Pitch defaultValue);

    Ui::MainWindow *ui = nullptr;

    PatliteDevice *m_device = nullptr;
    QTimer        *m_poll   = nullptr;

    std::array<QComboBox *, 5>   m_ledCombos{};
    std::array<QPushButton *, 5> m_ledButtons{};
    std::array<QPushButton *, 3> m_presetBtns{};

    // 미리보기용 상태 캐시 (장치는 read-back을 지원하지 않음)
    patlite::TowerState m_shadowTower;
    bool                m_shadowBuzzer = false;
};
