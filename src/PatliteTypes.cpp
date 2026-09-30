#include "PatliteTypes.h"

#include <QObject>

namespace patlite {

QString toString(Led v)
{
    switch (v) {
    case Led::Off:      return QObject::tr("소등");
    case Led::On:       return QObject::tr("점등");
    case Led::Pattern1: return QObject::tr("패턴1");
    case Led::Pattern2: return QObject::tr("패턴2");
    case Led::Pattern3: return QObject::tr("패턴3");
    case Led::Pattern4: return QObject::tr("패턴4");
    case Led::Keep:     return QObject::tr("유지");
    }
    return QObject::tr("알 수 없음");
}

QString toString(Buzzer v)
{
    switch (v) {
    case Buzzer::Off:      return QObject::tr("정지");
    case Buzzer::On:       return QObject::tr("연속");
    case Buzzer::Pattern1: return QObject::tr("패턴1");
    case Buzzer::Pattern2: return QObject::tr("패턴2");
    case Buzzer::Pattern3: return QObject::tr("패턴3");
    case Buzzer::Pattern4: return QObject::tr("패턴4");
    case Buzzer::Keep:     return QObject::tr("유지");
    }
    return QObject::tr("알 수 없음");
}

QString toString(Pitch v)
{
    switch (v) {
    case Pitch::Off:      return QObject::tr("정지");
    case Pitch::A6:       return QStringLiteral("A6");
    case Pitch::Bb6:      return QStringLiteral("B♭6");
    case Pitch::B6:       return QStringLiteral("B6");
    case Pitch::C7:       return QStringLiteral("C7");
    case Pitch::Db7:      return QStringLiteral("D♭7");
    case Pitch::D7:       return QStringLiteral("D7");
    case Pitch::Eb7:      return QStringLiteral("E♭7");
    case Pitch::E7:       return QStringLiteral("E7");
    case Pitch::F7:       return QStringLiteral("F7");
    case Pitch::Gb7:      return QStringLiteral("G♭7");
    case Pitch::G7:       return QStringLiteral("G7");
    case Pitch::Ab7:      return QStringLiteral("A♭7");
    case Pitch::A7:       return QStringLiteral("A7");
    case Pitch::DefaultA: return QObject::tr("기본 A (D7)");
    case Pitch::DefaultB: return QObject::tr("기본 B (정지)");
    }
    return QObject::tr("알 수 없음");
}

QString toString(Color v)
{
    switch (v) {
    case Color::Red:    return QObject::tr("적색");
    case Color::Yellow: return QObject::tr("황색");
    case Color::Green:  return QObject::tr("녹색");
    case Color::Blue:   return QObject::tr("청색");
    case Color::White:  return QObject::tr("백색");
    }
    return QObject::tr("알 수 없음");
}

} // namespace patlite
