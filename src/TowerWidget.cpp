#include "TowerWidget.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

using namespace patlite;

namespace {

constexpr int kTickMs = 100;

struct Segment {
    const char *label;
    QColor      color;
};

const Segment kSegments[] = {
    {"R", QColor(0xE0, 0x3A, 0x34)},
    {"Y", QColor(0xF2, 0xC2, 0x1B)},
    {"G", QColor(0x2F, 0xAE, 0x55)},
    {"B", QColor(0x2C, 0x79, 0xD8)},
    {"W", QColor(0xF0, 0xF0, 0xF0)},
};
constexpr int kSegmentCount = 5;

} // namespace

TowerWidget::TowerWidget(QWidget *parent)
    : QWidget(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, [this] {
        ++m_tick;
        update();
    });
    m_timer->start();
}

void TowerWidget::setState(const TowerState &state)
{
    m_state = state;
    m_tick  = 0;  // 패턴 위상 초기화
    update();
}

void TowerWidget::setBuzzerActive(bool active)
{
    if (m_buzzerActive == active)
        return;
    m_buzzerActive = active;
    update();
}

bool TowerWidget::isLitNow(Led pattern) const
{
    // 1 tick = 100ms
    switch (pattern) {
    case Led::On:
        return true;
    case Led::Pattern1:              // 약 1Hz 균등 점멸
        return (m_tick / 5) % 2 == 0;
    case Led::Pattern2:              // 약 2.5Hz 빠른 점멸
        return (m_tick / 2) % 2 == 0;
    case Led::Pattern3: {            // 더블 플래시
        const int p = m_tick % 12;
        return p < 2 || (p >= 4 && p < 6);
    }
    case Led::Pattern4: {            // 짧은 단발 플래시
        const int p = m_tick % 14;
        return p < 2;
    }
    case Led::Off:
    case Led::Keep:
    default:
        return false;
    }
}

void TowerWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const Led states[kSegmentCount] = {
        m_state.red, m_state.yellow, m_state.green, m_state.blue, m_state.white
    };

    const int margin    = 8;
    const int baseH     = 24;
    const int w         = width() - margin * 2;
    const int available = height() - margin * 2 - baseH;
    const int segH      = available / kSegmentCount;

    if (w <= 0 || segH <= 0)
        return;

    for (int i = 0; i < kSegmentCount; ++i) {
        const QRect r(margin, margin + i * segH, w, segH - 3);
        const bool lit = isLitNow(states[i]);

        QColor c = kSegments[i].color;
        if (!lit)
            c = c.darker(320);  // 소등 시 어둡게

        QLinearGradient grad(r.topLeft(), r.bottomLeft());
        grad.setColorAt(0.0, c.lighter(lit ? 125 : 105));
        grad.setColorAt(0.5, c);
        grad.setColorAt(1.0, c.darker(lit ? 115 : 105));

        QPainterPath path;
        path.addRoundedRect(r, 6, 6);
        p.fillPath(path, grad);

        p.setPen(QPen(QColor(0, 0, 0, 90), 1));
        p.drawPath(path);

        // 점등 중이면 색 이름을 살짝 표시
        p.setPen(lit ? QColor(0, 0, 0, 140) : QColor(255, 255, 255, 60));
        QFont f = p.font();
        f.setBold(true);
        f.setPointSizeF(f.pointSizeF() * 0.9);
        p.setFont(f);
        p.drawText(r, Qt::AlignCenter, QString::fromLatin1(kSegments[i].label));
    }

    // 베이스(부저 유닛)
    const QRect baseRect(margin, margin + kSegmentCount * segH, w, baseH - 2);
    QPainterPath basePath;
    basePath.addRoundedRect(baseRect, 4, 4);
    p.fillPath(basePath, QColor(0x3A, 0x3F, 0x45));
    p.setPen(QPen(QColor(0, 0, 0, 90), 1));
    p.drawPath(basePath);

    const bool buzzerBlink = m_buzzerActive && (m_tick / 3) % 2 == 0;
    p.setPen(buzzerBlink ? QColor(0xFF, 0xB3, 0x3A) : QColor(150, 150, 150));
    p.drawText(baseRect, Qt::AlignCenter,
               m_buzzerActive ? QStringLiteral("BUZZER") : QStringLiteral("PATLITE"));
}
