# rpi3-device-drivers
A collection of hardware device drivers developed for Raspberry Pi 3 on 64-bit Raspbian.

## 온습도 및 CO2 농도 측정기

Raspberry Pi 3 (64-bit Raspbian) 환경에서 **측정 센서(SCD41)** 와 **HD44780 LCD (I2C 확장 모듈 포함)**, **스위치 입력(GPIO)** 을 활용하여
공기질(온도, 습도, 이산화탄소 농도)을 측정하고 LCD에 표시하는 장치입니다.  

### 하드웨어 구성
- **Raspberry Pi 3** (64-bit Raspbian 커널 6.12.34)
- **Sensirion SCD41** 센서 (I²C, CO₂/온도/습도 측정)
- **HD44780 LCD** (I²C expander PCF8574 기반, 16x2 문자형 디스플레이)
- **Tactile Switch** (버튼 입력, GPIO 인터럽트 처리)

### 소프트웨어 구조
- **커널 드라이버**
  - `scd41` : I²C 기반 환경 센서 드라이버 (sysfs를 통해 측정값 제공)
  - `gpiosw` : GPIO 버튼 입력 드라이버 (인터럽트 발생 시 사용자 프로세스에 시그널 전달)
  - `hd44780` : I²C LCD 문자 디바이스 드라이버 (`/dev/hd44780`, `write`로 문자열 출력, `ioctl`로 제어)
- **유저 공간 프로그램**
  - 버튼 인터럽트를 시그널(`SIGUSR1`)로 수신
  - 버튼을 누르면 SCD41 측정 시작/중지 토글
  - 측정 주기마다 sysfs에서 센서 값을 읽어 LCD에 출력

### 동작 방식
1. 전원이 켜지면 LCD에 **"PRESS BUTTON TO START MEASURING"** 메시지가 표시됨  
2. 버튼을 누르면:
   - SCD41 센서 측정이 활성화됨 (`enable` sysfs 제어)
   - LCD에 "MEASURING…" 카운트다운 표시
3. 카운트다운 이후 LCD에는 현재 **온도(°C), 습도(%), CO₂(ppm)** 값이 실시간으로 표시됨  
4. 다시 버튼을 누르면 측정이 중지되고, 초기 대기 화면으로 복귀함

### 시연 영상

https://github.com/user-attachments/assets/7e9dca17-0ae5-4117-a68d-e80036f54538

