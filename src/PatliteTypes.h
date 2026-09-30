#pragma once

#include <QString>
#include <QtGlobal>

namespace patlite {

/// LED 유닛 색상 (프로토콜상 인덱스)
enum class Color : quint8 {
    Red    = 0,
    Yellow = 1,
    Green  = 2,
    Blue   = 3,
    White  = 4,
};

/// LED 점등 패턴.
/// 값이 LR6-USB의 4bit 필드와 LR5-LAN의 PNS 패턴 코드 모두에 그대로 대응되도록
/// 맞춰뒀다 (Off=0, On=1, Pattern1..4=2..5). Keep(0xF)만 장치별로 별도 매핑한다.
enum class Led : quint8 {
    Off      = 0x0,  ///< 소등
    On       = 0x1,  ///< 점등
    Pattern1 = 0x2,  ///< LR5 기준: 완만한 점멸
    Pattern2 = 0x3,  ///< LR5 기준: 보통 점멸
    Pattern3 = 0x4,  ///< LR5 기준: 빠른 점멸
    Pattern4 = 0x5,  ///< LR5 기준: 단발 플래시
    Keep     = 0xF,  ///< 현재 상태 유지
};

/// 부저 패턴 (4bit 필드). LR5-LAN은 Off/On/Keep만 지원한다.
enum class Buzzer : quint8 {
    Off      = 0x0,  ///< 정지
    On       = 0x1,  ///< 연속 취명
    Pattern1 = 0x2,
    Pattern2 = 0x3,
    Pattern3 = 0x4,
    Pattern4 = 0x5,
    Keep     = 0xF,  ///< 현재 상태 유지
};

/// 부저 음계 (4bit 필드). LR6-USB 전용.
enum class Pitch : quint8 {
    Off      = 0x0,
    A6       = 0x1,
    Bb6      = 0x2,
    B6       = 0x3,
    C7       = 0x4,
    Db7      = 0x5,
    D7       = 0x6,
    Eb7      = 0x7,
    E7       = 0x8,
    F7       = 0x9,
    Gb7      = 0xA,
    G7       = 0xB,
    Ab7      = 0xC,
    A7       = 0xD,
    DefaultA = 0xE,  ///< 음A 기본값 (D7)
    DefaultB = 0xF,  ///< 음B 기본값 (정지)
};

/// 5색 LED 상태 묶음
struct TowerState {
    Led red    = Led::Off;
    Led yellow = Led::Off;
    Led green  = Led::Off;
    Led blue   = Led::Off;
    Led white  = Led::Off;
};

QString toString(Led v);
QString toString(Buzzer v);
QString toString(Pitch v);
QString toString(Color v);

} // namespace patlite
