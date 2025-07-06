# TraceGo

TraceGo 배송 로봇 시스템의 핵심 모듈 코드입니다.

- 상품을 운반해주는 하드웨어의 중앙처리 모듈 코드 입니다.
- 주요 기능
    - 네트워크 연결
    - 외부에서 설정 접근
    - 내장 서버
    - RFID 연결
    - 보드간 통신

## 하드웨어
- 보드: ESP32-DEVKIT-V1
    - 모듈: RFID-RC522

## 설치

해당 리포지토리를 사용하려면 PlatformIO를 사용해야 합니다.

```
git clone https://github.com/oxxultus/tracego-core.git
```

- 빌드를 진행한 뒤 업로드를 진행하면됩니다.

### 종속성
- [tracego-server](https://github.com/oxxultus/tracego-server.git): `중앙 처리`
- [tracego-wheel](https://github.com/oxxultus/tracego-wheel.git): `이동 제어`
- [tracego-stand](https://github.com/oxxultus/tracego-stand.git): `상품 처리`
