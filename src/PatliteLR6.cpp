#include "PatliteLR6.h"

#include <libusb-1.0/libusb.h>

using namespace patlite;

namespace {

constexpr quint8 kCommandVersion = 0x00;
constexpr quint8 kCommandId      = 0x00;

inline quint8 nib(Led v)    { return static_cast<quint8>(v) & 0x0F; }
inline quint8 nib(Buzzer v) { return static_cast<quint8>(v) & 0x0F; }
inline quint8 nib(Pitch v)  { return static_cast<quint8>(v) & 0x0F; }

/// 헤더 2바이트 + 나머지 0으로 초기화된 프레임
PatliteLR6::Frame makeFrame()
{
    PatliteLR6::Frame f{};
    f.fill(0);
    f[0] = kCommandVersion;
    f[1] = kCommandId;
    return f;
}

/// LED 3바이트를 "현재 상태 유지"로 채운다.
void fillLedKeep(PatliteLR6::Frame &f)
{
    f[4] = static_cast<quint8>((nib(Led::Keep) << 4) | nib(Led::Keep));
    f[5] = static_cast<quint8>((nib(Led::Keep) << 4) | nib(Led::Keep));
    f[6] = static_cast<quint8>(nib(Led::Keep) << 4);  // 하위 4bit는 미사용
}

} // namespace

// ---------------------------------------------------------------------------

PatliteLR6::PatliteLR6(QObject *parent)
    : PatliteDevice(parent)
{
}

PatliteLR6::~PatliteLR6()
{
    teardown();
}

bool PatliteLR6::open(const QString & /*target*/)
{
    if (isOpen())
        return true;

    m_lastError.clear();

    int rc = libusb_init(&m_ctx);
    if (rc < 0) {
        m_ctx       = nullptr;
        m_lastError = tr("libusb 초기화 실패: %1").arg(QString::fromUtf8(libusb_error_name(rc)));
        emit logged(m_lastError);
        return false;
    }

    m_handle = libusb_open_device_with_vid_pid(m_ctx, kVendorId, kProductId);
    if (!m_handle) {
        m_lastError = tr("LR6-USB(VID 0x%1 / PID 0x%2)를 열 수 없습니다. "
                         "연결 상태 또는 udev 권한을 확인하세요.")
                          .arg(kVendorId, 4, 16, QLatin1Char('0'))
                          .arg(kProductId, 4, 16, QLatin1Char('0'));
        teardown();
        emit logged(m_lastError);
        return false;
    }

    // HID 클래스 장치라 커널의 usbhid가 먼저 물고 있는 것이 정상.
    // 원본 예제는 무조건 detach 하지만, 여기서는 실제로 붙어 있을 때만 떼고
    // close() 시점에 되돌려 놓는다.
    if (libusb_kernel_driver_active(m_handle, kInterface) == 1) {
        rc = libusb_detach_kernel_driver(m_handle, kInterface);
        if (rc != LIBUSB_SUCCESS) {
            m_lastError = tr("커널 드라이버 detach 실패: %1")
                              .arg(QString::fromUtf8(libusb_error_name(rc)));
            teardown();
            emit logged(m_lastError);
            return false;
        }
        m_detached = true;
    }

    rc = libusb_claim_interface(m_handle, kInterface);
    if (rc != LIBUSB_SUCCESS) {
        m_lastError = tr("인터페이스 claim 실패: %1")
                          .arg(QString::fromUtf8(libusb_error_name(rc)));
        teardown();
        emit logged(m_lastError);
        return false;
    }

    emit logged(tr("연결됨 — VID 0x%1 / PID 0x%2")
                    .arg(kVendorId, 4, 16, QLatin1Char('0'))
                    .arg(kProductId, 4, 16, QLatin1Char('0')));
    emit connectionChanged(true);
    return true;
}

void PatliteLR6::close()
{
    const bool wasOpen = isOpen();
    teardown();
    if (wasOpen) {
        emit logged(tr("연결 해제됨"));
        emit connectionChanged(false);
    }
}

void PatliteLR6::teardown()
{
    if (m_handle) {
        libusb_release_interface(m_handle, kInterface);
        if (m_detached)
            libusb_attach_kernel_driver(m_handle, kInterface);
        libusb_close(m_handle);
        m_handle = nullptr;
    }
    m_detached = false;

    if (m_ctx) {
        libusb_exit(m_ctx);
        m_ctx = nullptr;
    }
}

bool PatliteLR6::isDevicePresent()
{
    libusb_context *ctx = m_ctx;
    bool ownCtx = false;

    if (!ctx) {
        if (libusb_init(&ctx) < 0)
            return false;
        ownCtx = true;
    }

    libusb_device **list = nullptr;
    const ssize_t count  = libusb_get_device_list(ctx, &list);
    bool found = false;

    for (ssize_t i = 0; i < count && !found; ++i) {
        libusb_device_descriptor desc{};
        if (libusb_get_device_descriptor(list[i], &desc) != LIBUSB_SUCCESS)
            continue;
        found = (desc.idVendor == kVendorId && desc.idProduct == kProductId);
    }

    if (count >= 0)
        libusb_free_device_list(list, 1);
    if (ownCtx)
        libusb_exit(ctx);

    return found;
}

// ---------------------------------------------------------------------------
// 커맨드
// ---------------------------------------------------------------------------

bool PatliteLR6::setLight(Color color, Led pattern)
{
    Frame f = makeFrame();

    f[2] = nib(Buzzer::Keep);  // 부저 현재 상태 유지
    f[3] = 0;
    fillLedKeep(f);

    switch (color) {
    case Color::Red:
        f[4] = static_cast<quint8>((nib(pattern) << 4) | nib(Led::Keep));
        break;
    case Color::Yellow:
        f[4] = static_cast<quint8>((nib(Led::Keep) << 4) | nib(pattern));
        break;
    case Color::Green:
        f[5] = static_cast<quint8>((nib(pattern) << 4) | nib(Led::Keep));
        break;
    case Color::Blue:
        f[5] = static_cast<quint8>((nib(Led::Keep) << 4) | nib(pattern));
        break;
    case Color::White:
        f[6] = static_cast<quint8>(nib(pattern) << 4);
        break;
    default:
        m_lastError = tr("잘못된 LED 색상 인덱스");
        emit logged(m_lastError);
        return false;
    }

    return sendFrame(f, tr("SetLight(%1, %2)").arg(toString(color), toString(pattern)));
}

bool PatliteLR6::setTower(const TowerState &s)
{
    Frame f = makeFrame();

    f[2] = nib(Buzzer::Keep);
    f[3] = 0;
    f[4] = static_cast<quint8>((nib(s.red) << 4) | nib(s.yellow));
    f[5] = static_cast<quint8>((nib(s.green) << 4) | nib(s.blue));
    f[6] = static_cast<quint8>(nib(s.white) << 4);

    return sendFrame(f, tr("SetTower(R:%1 Y:%2 G:%3 B:%4 W:%5)")
                            .arg(toString(s.red), toString(s.yellow), toString(s.green),
                                 toString(s.blue), toString(s.white)));
}

bool PatliteLR6::setBuzzer(Buzzer pattern, quint8 limit)
{
    return setBuzzerEx(pattern, limit, Pitch::DefaultA, Pitch::DefaultB);
}

bool PatliteLR6::setBuzzerEx(Buzzer pattern, quint8 limit, Pitch pitchA, Pitch pitchB)
{
    Frame f = makeFrame();

    f[2] = static_cast<quint8>(((limit & 0x0F) << 4) | nib(pattern));
    f[3] = static_cast<quint8>((nib(pitchA) << 4) | nib(pitchB));
    fillLedKeep(f);

    return sendFrame(f, tr("SetBuz(%1, limit:%2, %3/%4)")
                            .arg(toString(pattern))
                            .arg(limit)
                            .arg(toString(pitchA), toString(pitchB)));
}

bool PatliteLR6::reset()
{
    Frame f = makeFrame();  // 전 필드 0 = LED 전소등 + 부저 정지
    return sendFrame(f, tr("Reset()"));
}

// ---------------------------------------------------------------------------

bool PatliteLR6::sendFrame(const Frame &frame, const QString &what)
{
    if (!isOpen()) {
        m_lastError = tr("장치가 연결되어 있지 않습니다.");
        emit logged(tr("%1 실패 — %2").arg(what, m_lastError));
        return false;
    }

    // libusb는 non-const 버퍼를 요구하므로 복사본을 만든다.
    Frame buf = frame;
    int transferred = 0;

    const int rc = libusb_interrupt_transfer(m_handle, kEndpointOut, buf.data(),
                                             kFrameSize, &transferred, kSendTimeoutMs);

    const QByteArray dump(reinterpret_cast<const char *>(frame.data()), kFrameSize);

    if (rc != LIBUSB_SUCCESS) {
        m_lastError = tr("전송 실패: %1").arg(QString::fromUtf8(libusb_error_name(rc)));
        emit logged(tr("%1 실패 — %2").arg(what, m_lastError));

        // 장치가 물리적으로 빠진 경우 상태를 정리한다.
        if (rc == LIBUSB_ERROR_NO_DEVICE || rc == LIBUSB_ERROR_IO) {
            teardown();
            emit connectionChanged(false);
        }
        return false;
    }

    if (transferred != kFrameSize) {
        m_lastError = tr("전송 바이트 수 불일치 (%1/%2)").arg(transferred).arg(kFrameSize);
        emit logged(tr("%1 실패 — %2").arg(what, m_lastError));
        return false;
    }

    emit frameSent(dump);
    emit logged(what);
    return true;
}

