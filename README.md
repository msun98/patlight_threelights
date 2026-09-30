# PATLITE LR6-USB Qt Controller

PATLITE 공식 예제 [`LR6-USB_linux_cpp_example`](https://github.com/PATLITE-Corporation/LR6-USB_linux_cpp_example)의
USB 프로토콜을 그대로 따르되, CLI 대신 Qt Widgets GUI로 재구성한 프로젝트입니다.
빌드는 CMake 기반이며 Qt6 / Qt5 모두 지원합니다.

## 구조

```
patlite-lr6-qt/
├── CMakeLists.txt
├── udev/99-patlite-lr6.rules      # sudo 없이 접근하기 위한 udev 규칙
└── src/
    ├── main.cpp
    ├── PatliteLR6.{h,cpp}         # 프로토콜 + libusb 래퍼 (UI 의존성 없음)
    ├── TowerWidget.{h,cpp}        # 신호등 상태 미리보기 위젯
    └── MainWindow.{h,cpp}         # 메인 UI
```

레이어를 나눈 이유는 `PatliteLR6`가 Qt Widgets에 전혀 의존하지 않기 때문입니다.
콘솔 툴이나 ROS 노드에 그대로 가져다 쓸 수 있고, `QObject`만 필요하므로
`Qt::Core`만 링크해도 동작합니다.

## 의존성

| 항목 | 최소 버전 | Ubuntu 패키지 |
|---|---|---|
| CMake | 3.16 | `cmake` |
| Qt Widgets | 5.15 / 6.x | `qt6-base-dev` 또는 `qtbase5-dev` |
| libusb | 1.0 | `libusb-1.0-0-dev` |
| pkg-config | — | `pkg-config` |

```bash
sudo apt install build-essential cmake pkg-config libusb-1.0-0-dev qt6-base-dev
```

## 빌드

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/patlite_lr6_qt
```

Qt6와 Qt5가 모두 설치된 환경에서 Qt5를 강제하려면:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt5
```

## 권한 설정

LR6-USB는 USB HID 클래스 장치라 커널의 `usbhid`가 먼저 점유합니다.
프로그램은 `libusb_detach_kernel_driver()`로 잠시 떼었다가 종료 시 되돌려 놓습니다.
이 동작에는 장치 노드 쓰기 권한이 필요하므로, `sudo` 없이 실행하려면 udev 규칙을 설치하세요.

```bash
sudo cp udev/99-patlite-lr6.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo usermod -aG plugdev $USER     # 로그아웃 후 재로그인
```

규칙 적용 후에는 장치를 한 번 뽑았다 다시 꽂아야 합니다.

## 프로토콜 요약

인터럽트 OUT 전송, 엔드포인트 `0x01`, 고정 8바이트.

| 바이트 | 내용 |
|---|---|
| 0 | 커맨드 버전 (0x00 고정) |
| 1 | 커맨드 ID (0x00 고정) |
| 2 | 상위 4bit = 취명 횟수(0=연속, 1~15) / 하위 4bit = 부저 패턴 |
| 3 | 상위 4bit = 음 A / 하위 4bit = 음 B |
| 4 | 상위 4bit = 적색 / 하위 4bit = 황색 |
| 5 | 상위 4bit = 녹색 / 하위 4bit = 청색 |
| 6 | 상위 4bit = 백색 / 하위 4bit = 미사용 |
| 7 | 예약 (0x00 고정) |

LED 패턴 값: `0x0` 소등, `0x1` 점등, `0x2`~`0x5` 패턴 1~4, `0xF` 현재 상태 유지.
부저 패턴 값도 동일한 범위를 씁니다.

GUI 로그창에 실제 전송 프레임이 헥사로 찍히므로, 예제 CLI 출력과 바이트 단위로 비교 검증할 수 있습니다.

## 원본 예제 대비 변경점

- **리소스 정리**: 원본은 `libusb_release_interface`, `libusb_exit` 호출이 빠져 있고 커널 드라이버를 재부착하지 않습니다. 이 프로젝트는 `teardown()`에서 release → attach → close → exit 순으로 정리합니다.
- **조건부 detach**: 원본은 무조건 `libusb_detach_kernel_driver()`를 부르지만, 여기서는 `libusb_kernel_driver_active()`로 확인 후 실제로 붙어 있을 때만 떼고, 우리가 뗀 경우에만 되돌립니다.
- **전용 컨텍스트**: `libusb_init(NULL)` 대신 인스턴스별 `libusb_context`를 사용해 같은 프로세스 안의 다른 libusb 사용자와 충돌하지 않습니다.
- **타입 안정성**: 매크로 상수를 `enum class`(`Color`, `Led`, `Buzzer`, `Pitch`)로 교체했습니다.
- **에러 처리**: `LIBUSB_ERROR_NO_DEVICE`/`LIBUSB_ERROR_IO` 발생 시 연결 상태를 정리하고 `connectionChanged(false)`를 방출합니다. 1초 주기 폴링으로 재연결도 자동 처리합니다.
- 원본 `main.cpp`의 `SendCommand` 선언은 `char*`, 정의는 `unsigned char*`로 불일치가 있었는데 여기서는 `std::array<quint8, 8>` 하나로 통일했습니다.

## 알려진 제약

- LR6-USB 프로토콜은 단방향(write-only)입니다. 장치의 실제 점등 상태를 읽어올 수 없으므로, 미리보기 위젯은 **마지막으로 전송한 명령을 기준으로 한 시뮬레이션**입니다. 점멸 주기도 근사값입니다.
- 전송 함수는 동기 호출입니다. 8바이트라 실제 지연은 1ms 미만이지만, 고빈도 반복 전송이 필요하면 `PatliteLR6` 인스턴스를 워커 `QThread`로 `moveToThread()` 해서 쓰세요. 이 클래스는 UI 의존성이 없어 그대로 옮겨집니다.
- 여러 대의 LR6-USB를 동시에 제어하려면 `libusb_open_device_with_vid_pid()` 대신 `libusb_get_device_list()`로 순회하며 시리얼/버스 주소로 구분해야 합니다.

## 라이선스

원본 예제 코드의 라이선스(PATLITE Corporation)를 확인한 뒤 사용하세요.
