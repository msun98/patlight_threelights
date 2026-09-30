#pragma once

#include <QWidget>

#include "PatliteTypes.h"

class QTimer;

/**
 * @brief 신호등 상태 미리보기 위젯.
 *
 * LR6-USB 프로토콜은 단방향(write-only)이라 실제 장치 상태를 읽어올 수 없다.
 * 따라서 이 위젯은 "마지막으로 전송한 명령"을 근거로 한 시뮬레이션이며,
 * 점멸 주기도 실제 하드웨어 패턴과 정확히 일치하지는 않는 근사값이다.
 */
class TowerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TowerWidget(QWidget *parent = nullptr);

    void setState(const patlite::TowerState &state);
    void setBuzzerActive(bool active);

    QSize sizeHint() const override { return QSize(140, 300); }
    QSize minimumSizeHint() const override { return QSize(110, 220); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    /// 100ms 틱 카운터를 기준으로 해당 패턴이 지금 켜져 있어야 하는지 판정
    bool isLitNow(patlite::Led pattern) const;

    patlite::TowerState m_state;
    bool    m_buzzerActive = false;
    QTimer *m_timer        = nullptr;
    int     m_tick         = 0;
};
