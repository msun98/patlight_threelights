#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>

#include "PatliteLR5Lan.h"
#include "PatliteLR6.h"
#include "TowerWidget.h"

using namespace patlite;

namespace {

constexpr int kPollIntervalMs = 1000;

const Color kColorOrder[5] = {
    Color::Red, Color::Yellow, Color::Green, Color::Blue, Color::White
};

Led ledFrom(const QComboBox *box)
{
    return static_cast<Led>(box->currentData().toUInt());
}

Buzzer buzzerFrom(const QComboBox *box)
{
    return static_cast<Buzzer>(box->currentData().toUInt());
}

Pitch pitchFrom(const QComboBox *box)
{
    return static_cast<Pitch>(box->currentData().toUInt());
}

} // namespace

// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_ledCombos  = {ui->ledCombo_0, ui->ledCombo_1, ui->ledCombo_2, ui->ledCombo_3, ui->ledCombo_4};
    m_ledButtons = {ui->ledButton_0, ui->ledButton_1, ui->ledButton_2, ui->ledButton_3, ui->ledButton_4};
    m_presetBtns = {ui->presetBtn_0, ui->presetBtn_1, ui->presetBtn_2};

    for (int i = 0; i < 5; ++i) {
        fillLedCombo(m_ledCombos[i], /*includeKeep=*/true);
        connect(m_ledButtons[i], &QPushButton::clicked, this, [this, i] { onApplySingle(i); });
    }

    fillBuzzerCombo(ui->buzPattern);
    fillPitchCombo(ui->buzPitchA, Pitch::DefaultA);
    fillPitchCombo(ui->buzPitchB, Pitch::DefaultB);

    ui->deviceType->addItem(tr("LR6-USB"), 0);
    ui->deviceType->addItem(tr("LR5-LAN"), 1);

    ui->lanTarget->setPlaceholderText(PatliteLR5Lan::defaultTarget());
    ui->lanTarget->setText(PatliteLR5Lan::defaultTarget());
    ui->lanTarget->setVisible(false);

    ui->log->setMaximumBlockCount(2000);
    ui->log->setFont(QFont(QStringLiteral("Monospace"), 9));
    ui->log->document()->setDefaultFont(QFont(QStringLiteral("Monospace"), 9));

    connect(ui->deviceType, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDeviceTypeChanged);
    connect(ui->connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->towerBtn, &QPushButton::clicked, this, &MainWindow::onApplyTower);
    connect(ui->buzApply, &QPushButton::clicked, this, &MainWindow::onApplyBuzzer);
    connect(ui->buzStop, &QPushButton::clicked, this, &MainWindow::onStopBuzzer);
    connect(ui->resetBtn, &QPushButton::clicked, this, &MainWindow::onReset);
    connect(ui->clearLogBtn, &QPushButton::clicked, ui->log, &QPlainTextEdit::clear);

    connect(ui->buzUsePitch, &QCheckBox::toggled, this, [this](bool on) {
        ui->buzPitchA->setEnabled(on && m_device->isOpen());
        ui->buzPitchB->setEnabled(on && m_device->isOpen());
    });

    connect(ui->presetBtn_0, &QPushButton::clicked, this, [this] {
        TowerState s;
        s.green = Led::On;
        applyPreset(s, Buzzer::Off, 0);
    });
    connect(ui->presetBtn_1, &QPushButton::clicked, this, [this] {
        TowerState s;
        s.yellow = Led::Pattern1;
        applyPreset(s, Buzzer::Pattern1, 3);
    });
    connect(ui->presetBtn_2, &QPushButton::clicked, this, [this] {
        TowerState s;
        s.red = Led::Pattern2;
        applyPreset(s, Buzzer::On, 0);
    });

    statusBar()->showMessage(tr("준비"));

    setActiveDevice(new PatliteLR6(this));

    m_poll = new QTimer(this);
    m_poll->setInterval(kPollIntervalMs);
    connect(m_poll, &QTimer::timeout, this, &MainWindow::onPoll);
    m_poll->start();

    updateEnabledState();
    resize(880, 720);
}

MainWindow::~MainWindow()
{
    // m_device는 this를 부모로 둔 QObject 자식이라 원래는 ~MainWindow() 이후에
    // 자동 파괴되는데, 그 시점엔 이미 ui가 delete된 뒤라 device 소멸자의
    // close()가 내보내는 connectionChanged/logged 시그널이 dangling ui를
    // 참조해서 크래시가 난다. ui를 지우기 전에 먼저 끊고 정리한다.
    if (m_device) {
        m_device->disconnect(this);
        m_device->close();
        delete m_device;
        m_device = nullptr;
    }
    delete ui;
}

// ---------------------------------------------------------------------------
// 콤보박스 채우기
// ---------------------------------------------------------------------------

void MainWindow::fillLedCombo(QComboBox *box, bool includeKeep)
{
    const Led values[] = {Led::Off, Led::On, Led::Pattern1, Led::Pattern2,
                          Led::Pattern3, Led::Pattern4};
    for (Led v : values)
        box->addItem(toString(v), static_cast<uint>(v));
    if (includeKeep)
        box->addItem(toString(Led::Keep), static_cast<uint>(Led::Keep));
}

void MainWindow::fillBuzzerCombo(QComboBox *box)
{
    const Buzzer values[] = {Buzzer::Off, Buzzer::On, Buzzer::Pattern1, Buzzer::Pattern2,
                             Buzzer::Pattern3, Buzzer::Pattern4};
    for (Buzzer v : values)
        box->addItem(toString(v), static_cast<uint>(v));
    box->setCurrentIndex(1);  // 기본 '연속'
}

void MainWindow::fillPitchCombo(QComboBox *box, Pitch defaultValue)
{
    for (uint raw = 0x0; raw <= 0xF; ++raw) {
        const Pitch v = static_cast<Pitch>(raw);
        box->addItem(toString(v), raw);
    }
    box->setCurrentIndex(static_cast<int>(defaultValue));
}

// ---------------------------------------------------------------------------
// 동작
// ---------------------------------------------------------------------------

void MainWindow::setActiveDevice(PatliteDevice *device)
{
    if (m_device == device)
        return;

    if (m_device) {
        m_device->close();
        m_device->disconnect(this);
        m_device->deleteLater();
    }

    m_device = device;
    connect(m_device, &PatliteDevice::connectionChanged, this, &MainWindow::onConnectionChanged);
    connect(m_device, &PatliteDevice::logged, this, &MainWindow::appendLog);
    connect(m_device, &PatliteDevice::frameSent, this, &MainWindow::appendFrame);

    onConnectionChanged(false);
}

void MainWindow::onDeviceTypeChanged(int index)
{
    if (m_device && m_device->isOpen()) {
        // 연결된 상태에서 장치 종류를 바꾸면 먼저 해제한다.
        m_device->close();
    }

    ui->lanTarget->setVisible(index == 1);

    PatliteDevice *device = (index == 1) ? static_cast<PatliteDevice *>(new PatliteLR5Lan(this))
                                          : static_cast<PatliteDevice *>(new PatliteLR6(this));
    setActiveDevice(device);
}

void MainWindow::onConnectClicked()
{
    if (m_device->isOpen()) {
        m_device->close();
        return;
    }

    const bool isLan = ui->deviceType->currentData().toInt() == 1;
    const QString target = isLan ? ui->lanTarget->text().trimmed() : QString();
    if (!m_device->open(target))
        statusBar()->showMessage(m_device->lastError(), 5000);
}

void MainWindow::onConnectionChanged(bool connected)
{
    ui->statusDot->setStyleSheet(QStringLiteral("color:%1; font-size:18px;")
                                     .arg(connected ? QStringLiteral("#27AE60")
                                                    : QStringLiteral("#C0392B")));
    ui->statusText->setText(connected ? tr("연결됨") : tr("연결 안 됨"));
    ui->connectBtn->setText(connected ? tr("연결 해제") : tr("연결"));

    if (!connected) {
        m_shadowTower  = TowerState{};
        m_shadowBuzzer = false;
        ui->preview->setState(m_shadowTower);
        ui->preview->setBuzzerActive(false);
    }
    updateEnabledState();
}

void MainWindow::onPoll()
{
    if (m_device->isOpen())
        return;
    if (!ui->autoConnect->isChecked())
        return;
    if (!m_device->isDevicePresent())
        return;

    // 장치가 다시 꽂혔고 자동 재연결이 켜져 있으면 조용히 연결 시도 (LR6-USB만 지원).
    m_device->open();
}

void MainWindow::onApplySingle(int colorIndex)
{
    const Led pattern = ledFrom(m_ledCombos[colorIndex]);

    TowerState s;
    s.red = s.yellow = s.green = s.blue = s.white = Led::Keep;
    switch (kColorOrder[colorIndex]) {
    case Color::Red:    s.red    = pattern; break;
    case Color::Yellow: s.yellow = pattern; break;
    case Color::Green:  s.green  = pattern; break;
    case Color::Blue:   s.blue   = pattern; break;
    case Color::White:  s.white  = pattern; break;
    }

    if (!m_device->setTower(s))
        return;

    if (pattern != Led::Keep) {
        switch (kColorOrder[colorIndex]) {
        case Color::Red:    m_shadowTower.red    = pattern; break;
        case Color::Yellow: m_shadowTower.yellow = pattern; break;
        case Color::Green:  m_shadowTower.green  = pattern; break;
        case Color::Blue:   m_shadowTower.blue   = pattern; break;
        case Color::White:  m_shadowTower.white  = pattern; break;
        }
        ui->preview->setState(m_shadowTower);
    }
}

TowerState MainWindow::currentTowerSelection() const
{
    TowerState s;
    s.red    = ledFrom(m_ledCombos[0]);
    s.yellow = ledFrom(m_ledCombos[1]);
    s.green  = ledFrom(m_ledCombos[2]);
    s.blue   = ledFrom(m_ledCombos[3]);
    s.white  = ledFrom(m_ledCombos[4]);
    return s;
}

void MainWindow::onApplyTower()
{
    const TowerState s = currentTowerSelection();
    if (!m_device->setTower(s))
        return;

    // Keep 항목은 그림자 상태를 유지
    if (s.red    != Led::Keep) m_shadowTower.red    = s.red;
    if (s.yellow != Led::Keep) m_shadowTower.yellow = s.yellow;
    if (s.green  != Led::Keep) m_shadowTower.green  = s.green;
    if (s.blue   != Led::Keep) m_shadowTower.blue   = s.blue;
    if (s.white  != Led::Keep) m_shadowTower.white  = s.white;
    ui->preview->setState(m_shadowTower);
}

void MainWindow::onApplyBuzzer()
{
    const Buzzer pattern = buzzerFrom(ui->buzPattern);
    const quint8 limit   = static_cast<quint8>(ui->buzLimit->value());

    auto *usb = qobject_cast<PatliteLR6 *>(m_device);
    const bool ok = (usb && ui->buzUsePitch->isChecked())
                        ? usb->setBuzzerEx(pattern, limit,
                                           pitchFrom(ui->buzPitchA),
                                           pitchFrom(ui->buzPitchB))
                        : m_device->setBuzzer(pattern, limit);
    if (!ok)
        return;

    m_shadowBuzzer = (pattern != Buzzer::Off);
    ui->preview->setBuzzerActive(m_shadowBuzzer);
}

void MainWindow::onStopBuzzer()
{
    if (!m_device->setBuzzer(Buzzer::Off, 0))
        return;
    m_shadowBuzzer = false;
    ui->preview->setBuzzerActive(false);
}

void MainWindow::onReset()
{
    if (!m_device->reset())
        return;
    m_shadowTower  = TowerState{};
    m_shadowBuzzer = false;
    ui->preview->setState(m_shadowTower);
    ui->preview->setBuzzerActive(false);
}

void MainWindow::applyPreset(const TowerState &tower, Buzzer buzzer, quint8 limit)
{
    if (!m_device->setTower(tower))
        return;
    m_shadowTower = tower;
    ui->preview->setState(m_shadowTower);

    if (!m_device->setBuzzer(buzzer, limit))
        return;
    m_shadowBuzzer = (buzzer != Buzzer::Off);
    ui->preview->setBuzzerActive(m_shadowBuzzer);
}

void MainWindow::updateEnabledState()
{
    const bool on    = m_device->isOpen();
    const bool isUsb = qobject_cast<PatliteLR6 *>(m_device) != nullptr;

    ui->deviceType->setEnabled(!on);
    ui->lanTarget->setEnabled(!on);

    for (auto *c : m_ledCombos)  c->setEnabled(on);
    for (auto *b : m_ledButtons) b->setEnabled(on);
    for (auto *b : m_presetBtns) b->setEnabled(on);

    ui->towerBtn->setEnabled(on);
    ui->buzPattern->setEnabled(on);
    ui->buzLimit->setEnabled(on && isUsb);
    ui->buzUsePitch->setEnabled(on && isUsb);
    ui->buzPitchA->setEnabled(on && isUsb && ui->buzUsePitch->isChecked());
    ui->buzPitchB->setEnabled(on && isUsb && ui->buzUsePitch->isChecked());
    ui->buzApply->setEnabled(on);
    ui->buzStop->setEnabled(on);
    ui->resetBtn->setEnabled(on);
}

// ---------------------------------------------------------------------------
// 로그
// ---------------------------------------------------------------------------

void MainWindow::appendLog(const QString &message)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    ui->log->appendPlainText(QStringLiteral("[%1] %2").arg(ts, message));
    statusBar()->showMessage(message, 4000);
}

void MainWindow::appendFrame(const QByteArray &frame)
{
    QStringList hex;
    hex.reserve(frame.size());
    for (char c : frame) {
        hex << QStringLiteral("%1").arg(static_cast<quint8>(c), 2, 16, QLatin1Char('0'))
                   .toUpper();
    }
    ui->log->appendPlainText(QStringLiteral("           TX: %1").arg(hex.join(QLatin1Char(' '))));
}
