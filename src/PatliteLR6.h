#pragma once

#include <QByteArray>

#include <array>

#include "PatliteDevice.h"
#include "PatliteTypes.h"

struct libusb_context;
struct libusb_device_handle;

/**
 * @brief PATLITE LR6-USB 적층 신호등 제어 클래스.
 *
 * PATLITE 공식 Linux C++ 예제(LR6-USB_linux_cpp_example)의 프로토콜을 그대로 따르되,
 * 리소스 관리(claim/release, kernel driver 재부착, libusb_exit)를 RAII 형태로 정리하고
 * Qt 시그널로 로그/연결 상태를 노출한다.
 *
 * 전송 프레임(8 byte, interrupt OUT / endpoint 0x01):
 *   [0] 커맨드 버전 (0x00 고정)
 *   [1] 커맨드 ID   (0x00 고정)
 *   [2] 부저 제어   : 상위 4bit = 취명 횟수(0=연속, 1~15), 하위 4bit = 부저 패턴
 *   [3] 부저 음계   : 상위 4bit = 음A, 하위 4bit = 음B
 *   [4] LED 제어    : 상위 4bit = 적색, 하위 4bit = 황색
 *   [5] LED 제어    : 상위 4bit = 녹색, 하위 4bit = 청색
 *   [6] LED 제어    : 상위 4bit = 백색, 하위 4bit = 미사용(0)
 *   [7] 예약        (0x00 고정)
 *
 * @note 모든 전송 함수는 동기(blocking)이며 최대 kSendTimeoutMs 만큼 대기한다.
 *       8바이트 인터럽트 전송이라 실제로는 1ms 미만이지만, 대량 반복 전송이
 *       필요하면 이 객체를 워커 QThread로 moveToThread 해서 사용할 것.
 */
class PatliteLR6 : public PatliteDevice
{
    Q_OBJECT

public:
    static constexpr quint16 kVendorId      = 0x191A;  ///< PATLITE
    static constexpr quint16 kProductId     = 0x8003;  ///< LR6-USB
    static constexpr int     kInterface     = 0;
    static constexpr quint8  kEndpointOut   = 0x01;
    static constexpr int     kSendTimeoutMs = 1000;
    static constexpr int     kFrameSize     = 8;

    using Frame = std::array<quint8, kFrameSize>;

    explicit PatliteLR6(QObject *parent = nullptr);
    ~PatliteLR6() override;

    /// target은 사용하지 않는다 (USB는 VID/PID로 자동 탐색).
    bool open(const QString &target = QString()) override;
    /// 장치 해제 (인터페이스 release + 커널 드라이버 재부착 포함).
    void close() override;
    bool isOpen() const override { return m_handle != nullptr; }

    /// 현재 USB 버스에 LR6-USB가 존재하는지 확인 (연결 없이 스캔만).
    bool isDevicePresent() override;

    // --- 프로토콜 커맨드 ---------------------------------------------------

    /// 단일 색상만 제어. 나머지 LED와 부저는 현재 상태 유지.
    bool setLight(patlite::Color color, patlite::Led pattern);

    /// 5색 LED를 한 번에 제어. 부저는 현재 상태 유지.
    bool setTower(const patlite::TowerState &state) override;

    /// 부저 제어(기본 음계). LED는 현재 상태 유지.
    /// @param limit 0 = 연속, 1~15 = 취명 횟수
    bool setBuzzer(patlite::Buzzer pattern, quint8 limit) override;

    /// 부저 제어(음계 지정). LED는 현재 상태 유지.
    bool setBuzzerEx(patlite::Buzzer pattern, quint8 limit,
                     patlite::Pitch pitchA, patlite::Pitch pitchB);

    /// 전체 LED 소등 + 부저 정지.
    bool reset() override;

private:
    bool sendFrame(const Frame &frame, const QString &what);
    void teardown();

    libusb_context       *m_ctx      = nullptr;
    libusb_device_handle *m_handle   = nullptr;
    bool                  m_detached = false;  ///< 커널 드라이버를 우리가 뗐는지
};
