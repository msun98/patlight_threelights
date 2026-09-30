#include "PatliteLR5Lan.h"

#include <QByteArray>

#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace patlite;

namespace {

constexpr quint8 kProductCategoryHi = 0x41;
constexpr quint8 kProductCategoryLo = 0x42;
constexpr quint8 kCmdRunControl     = 'S';
constexpr quint8 kCmdClear          = 'C';
constexpr quint8 kNak               = 0x15;

/// patlite::Led -> LR5 PNS LED 코드. Off/On/Pattern1..4는 값이 그대로 겹치고,
/// Keep(0xF)만 LR5의 "변경 없음"(0x09)으로 매핑한다.
quint8 toLr5Led(Led v)
{
    if (v == Led::Keep)
        return 0x09;
    return static_cast<quint8>(v);
}

/// patlite::Buzzer -> LR5 PNS 부저 코드 (정지/연속/변경없음만 지원).
quint8 toLr5Buzzer(Buzzer v)
{
    switch (v) {
    case Buzzer::Off:  return 0x00;
    case Buzzer::Keep: return 0x09;
    default:           return 0x01;  // On/Pattern1..4 -> 연속 취명 (LR5는 패턴 미지원)
    }
}

QString errnoString()
{
    return QString::fromLocal8Bit(std::strerror(errno));
}

} // namespace

PatliteLR5Lan::PatliteLR5Lan(QObject *parent)
    : PatliteDevice(parent)
{
}

PatliteLR5Lan::~PatliteLR5Lan()
{
    close();
}

bool PatliteLR5Lan::open(const QString &target)
{
    if (isOpen())
        return true;

    m_lastError.clear();

    const QString t    = target.isEmpty() ? defaultTarget() : target;
    QString       host = t;
    quint16       port = kDefaultPort;

    const int idx = t.indexOf(QLatin1Char(':'));
    if (idx >= 0) {
        host = t.left(idx);
        bool      ok = false;
        const int p  = t.mid(idx + 1).toInt(&ok);
        if (ok && p > 0 && p <= 65535)
            port = static_cast<quint16>(p);
    }

    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *res = nullptr;
    const QByteArray hostUtf8 = host.toUtf8();
    const QByteArray portUtf8 = QByteArray::number(port);
    const int gaiRc = getaddrinfo(hostUtf8.constData(), portUtf8.constData(), &hints, &res);
    if (gaiRc != 0) {
        m_lastError = tr("주소 확인 실패: %1").arg(QString::fromLocal8Bit(gai_strerror(gaiRc)));
        emit logged(m_lastError);
        return false;
    }

    int fd = -1;
    for (addrinfo *p = res; p; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, p->ai_protocol);
        if (fd < 0)
            continue;

        int rc = ::connect(fd, p->ai_addr, p->ai_addrlen);
        if (rc == 0)
            break;  // 즉시 연결됨 (드묾)

        if (errno != EINPROGRESS) {
            ::close(fd);
            fd = -1;
            continue;
        }

        pollfd pfd{fd, POLLOUT, 0};
        rc = ::poll(&pfd, 1, kConnectTimeoutMs);
        if (rc <= 0) {
            ::close(fd);
            fd = -1;
            continue;
        }

        int       sockErr = 0;
        socklen_t len      = sizeof(sockErr);
        if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &sockErr, &len) < 0 || sockErr != 0) {
            ::close(fd);
            fd = -1;
            continue;
        }
        break;  // 연결 성공
    }
    freeaddrinfo(res);

    if (fd < 0) {
        m_lastError = tr("LR5-LAN(%1:%2) 연결 실패: %3").arg(host).arg(port).arg(errnoString());
        emit logged(m_lastError);
        return false;
    }

    // 연결 후에는 blocking 모드로 되돌리고, 송수신은 poll()로 개별 타임아웃을 건다.
    const int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    const int one = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    m_fd = fd;

    emit logged(tr("연결됨 — %1:%2").arg(host).arg(port));
    emit connectionChanged(true);
    return true;
}

void PatliteLR5Lan::close()
{
    const bool wasOpen = isOpen();

    if (m_fd >= 0) {
        ::shutdown(m_fd, SHUT_RDWR);
        ::close(m_fd);
        m_fd = -1;
    }

    if (wasOpen) {
        emit logged(tr("연결 해제됨"));
        emit connectionChanged(false);
    }
}

bool PatliteLR5Lan::writeAll(const char *data, std::size_t len)
{
    std::size_t sent = 0;
    while (sent < len) {
        pollfd pfd{m_fd, POLLOUT, 0};
        const int rc = ::poll(&pfd, 1, kIoTimeoutMs);
        if (rc <= 0)
            return false;

        const ssize_t n = ::send(m_fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (n == 0)
            return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

bool PatliteLR5Lan::readExact(char *buf, std::size_t len)
{
    std::size_t got = 0;
    while (got < len) {
        pollfd pfd{m_fd, POLLIN, 0};
        const int rc = ::poll(&pfd, 1, kIoTimeoutMs);
        if (rc <= 0)
            return false;

        const ssize_t n = ::recv(m_fd, buf + got, len - got, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (n == 0)
            return false;  // 상대가 연결을 닫음
        got += static_cast<std::size_t>(n);
    }
    return true;
}

// ---------------------------------------------------------------------------
// 커맨드
// ---------------------------------------------------------------------------

bool PatliteLR5Lan::setTower(const TowerState &state)
{
    return sendRunControl(state, Buzzer::Keep,
                          tr("SetTower(R:%1 Y:%2 G:%3 B:%4 W:%5)")
                              .arg(toString(state.red), toString(state.yellow),
                                   toString(state.green), toString(state.blue),
                                   toString(state.white)));
}

bool PatliteLR5Lan::setBuzzer(Buzzer pattern, quint8 /*limit*/)
{
    TowerState keep;  // 전부 Led::Keep (기본값이 Off라서 명시적으로 채운다)
    keep.red = keep.yellow = keep.green = keep.blue = keep.white = Led::Keep;
    return sendRunControl(keep, pattern, tr("SetBuz(%1)").arg(toString(pattern)));
}

bool PatliteLR5Lan::reset()
{
    return sendCommand(kCmdClear, QByteArray(), tr("Reset()"));
}

bool PatliteLR5Lan::sendRunControl(const TowerState &led, Buzzer buzzer, const QString &what)
{
    QByteArray data;
    data.append(static_cast<char>(toLr5Led(led.red)));
    data.append(static_cast<char>(toLr5Led(led.yellow)));
    data.append(static_cast<char>(toLr5Led(led.green)));
    data.append(static_cast<char>(toLr5Led(led.blue)));
    data.append(static_cast<char>(toLr5Led(led.white)));
    data.append(static_cast<char>(toLr5Buzzer(buzzer)));

    return sendCommand(kCmdRunControl, data, what);
}

bool PatliteLR5Lan::sendCommand(quint8 commandId, const QByteArray &data, const QString &what)
{
    if (!isOpen()) {
        m_lastError = tr("장치가 연결되어 있지 않습니다.");
        emit logged(tr("%1 실패 — %2").arg(what, m_lastError));
        return false;
    }

    QByteArray frame;
    frame.reserve(6 + data.size());
    frame.append(static_cast<char>(kProductCategoryHi));
    frame.append(static_cast<char>(kProductCategoryLo));
    frame.append(static_cast<char>(commandId));
    frame.append(static_cast<char>(0x00));
    frame.append(static_cast<char>((data.size() >> 8) & 0xFF));
    frame.append(static_cast<char>(data.size() & 0xFF));
    frame.append(data);

    if (!writeAll(frame.constData(), static_cast<std::size_t>(frame.size()))) {
        m_lastError = tr("전송 실패: %1").arg(errnoString());
        emit logged(tr("%1 실패 — %2").arg(what, m_lastError));
        close();
        return false;
    }

    char ack = 0;
    if (!readExact(&ack, 1)) {
        m_lastError = tr("응답 없음(타임아웃 또는 연결 끊김)");
        emit logged(tr("%1 실패 — %2").arg(what, m_lastError));
        close();
        return false;
    }

    if (static_cast<quint8>(ack) == kNak) {
        m_lastError = tr("장치가 NAK 응답을 반환했습니다.");
        emit logged(tr("%1 실패 — %2").arg(what, m_lastError));
        return false;
    }

    emit frameSent(frame);
    emit logged(what);
    return true;
}
