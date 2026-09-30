#pragma once

#include "PatliteDevice.h"
#include "PatliteTypes.h"

#include <cstddef>

/**
 * @brief PATLITE LR5-LAN 적층 신호등 제어 클래스 (Linux 소켓, PNS 프로토콜).
 *
 * Qt의 QTcpSocket 대신 POSIX 소켓 API(socket/connect/poll/send/recv)를 직접 사용한다.
 * 연결은 넌블로킹 connect() + poll()로 타임아웃을 걸고, 성공 후에는 블로킹 모드로
 * 되돌려서 송수신마다 poll()로 타임아웃을 건다 (LR6-USB의 동기식 설계와 동일한 스타일).
 *
 * PATLITE 공식 예제(LR5-LAN_windows_cpp_example)의 PNS 커맨드를 그대로 따른다.
 * 기본 접속 정보는 192.168.10.1:10000 (장치 출고 기본값).
 *
 * 커맨드 프레임: [0-1] 제품 카테고리(0x41 0x42) [2] 커맨드ID [3] 0(고정)
 *               [4-5] 데이터 크기(빅엔디안) [6..] 데이터
 *   - 'S' 운전 제어: 데이터 6바이트 = R/Amber/G/B/W LED 패턴 + 부저 모드
 *   - 'C' 클리어   : 데이터 없음 (LED 전소등 + 부저 정지)
 * 응답은 1바이트 ACK(0x06)/NAK(0x15).
 *
 * @note patlite::Led/Buzzer 값은 LR6-USB와 공유하는 공통 열거형이며,
 *       Off/On/Pattern1..4 값이 LR5의 점멸/플래시 코드(0,1,2..5)와 그대로 겹치도록
 *       설계되어 있다. Keep(0xF)만 LR5의 "변경 없음"(0x09)으로 별도 매핑한다.
 *       LR5 부저는 정지/연속만 지원하므로 Pattern1..4는 연속(On)으로 취급한다.
 */
class PatliteLR5Lan : public PatliteDevice
{
    Q_OBJECT

public:
    static constexpr quint16 kDefaultPort      = 10000;
    static constexpr int     kConnectTimeoutMs = 3000;
    static constexpr int     kIoTimeoutMs      = 2000;

    static QString defaultTarget() { return QStringLiteral("192.168.10.1:10000"); }

    explicit PatliteLR5Lan(QObject *parent = nullptr);
    ~PatliteLR5Lan() override;

    /// target 형식: "host" 또는 "host:port". 비어 있으면 defaultTarget() 사용.
    bool open(const QString &target = QString()) override;
    void close() override;
    bool isOpen() const override { return m_fd >= 0; }

    bool setTower(const patlite::TowerState &state) override;
    bool setBuzzer(patlite::Buzzer pattern, quint8 limit) override;
    bool reset() override;

private:
    bool sendCommand(quint8 commandId, const QByteArray &data, const QString &what);
    bool sendRunControl(const patlite::TowerState &led, patlite::Buzzer buzzer, const QString &what);

    bool writeAll(const char *data, std::size_t len);
    bool readExact(char *buf, std::size_t len);

    int m_fd = -1;
};
