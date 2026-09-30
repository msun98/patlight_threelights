#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include "PatliteTypes.h"

/**
 * @brief PATLITE 적층 신호등 백엔드 공통 인터페이스.
 *
 * LR6-USB(libusb)와 LR5-LAN(TCP) 두 장치를 GUI가 동일하게 다룰 수 있도록
 * 최소 공통분모(5색 LED 일괄 제어, 부저 On/Off, 리셋)를 추상화한다.
 * 장치별 세부 기능(LR6의 부저 음계 지정 등)은 파생 클래스에 별도로 남겨두고,
 * MainWindow에서 dynamic_cast로 선택적으로 사용한다.
 */
class PatliteDevice : public QObject
{
    Q_OBJECT

public:
    ~PatliteDevice() override = default;

    PatliteDevice(const PatliteDevice &)            = delete;
    PatliteDevice &operator=(const PatliteDevice &) = delete;

    /// 장치 연결. target 해석은 백엔드마다 다르다 (USB는 무시, LAN은 "host:port").
    virtual bool open(const QString &target = QString()) = 0;
    virtual void close()                                 = 0;
    virtual bool isOpen() const                          = 0;

    /// 연결 없이 장치 존재 여부를 확인할 수 있으면 true (자동 재연결용). 기본은 미지원.
    virtual bool isDevicePresent() { return false; }

    QString lastError() const { return m_lastError; }

    virtual bool setTower(const patlite::TowerState &state)          = 0;
    virtual bool setBuzzer(patlite::Buzzer pattern, quint8 limit)    = 0;
    virtual bool reset()                                             = 0;

signals:
    void connectionChanged(bool connected);
    void logged(const QString &message);
    void frameSent(const QByteArray &frame);

protected:
    explicit PatliteDevice(QObject *parent = nullptr) : QObject(parent) {}

    QString m_lastError;
};
