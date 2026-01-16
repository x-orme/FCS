# K105A1 FCS (Fire Control System) - Architecture Deep Dive

**작성일:** 2026-01-13  
**작성자:** Project Lead (Assisted by AI)

---

## 1. 개요 (Overview)
본 문서는 K105A1 자주포 사격 통제 시스템(FCS)의 펌웨어 아키텍처와 핵심 알고리즘을 기술적으로 깊이 있게 분석한 자료입니다. 단순한 기능 구현을 넘어, **실시간성(Real-time)**과 **유지보수성(Maintainability)**을 고려한 임베디드 소프트웨어 설계 철학을 담고 있습니다.

---

## 2. 소프트웨어 아키텍처 (Software Architecture)

### 2.1 Layered Architecture (계층형 구조)
기존의 Monolithic(통짜) 코드를 탈피하여, 하드웨어 추상화 계층(HAL) 위에 논리적인 기능을 분리하였습니다.

| Layer | Module | Description |
|:---:|:---:|---|
| **App** | `main.c` / `ui.c` | UI 상태 관리, 사용자 인터랙션, 전체 루프 스케줄링 |
| **Domain** | `fcs_core.c` | **비즈니스 로직의 핵심.** 좌표 변환, 사격 제원 산출 요청, 통신 프로토콜 처리 |
| **Logic** | `fcs_math.c` | 순수 수학/물리 연산 (사표 보간, UTM 변환). 하드웨어 종속성 없음 (Pure C) |
| **Driver** | `input.c` / `bmp280.c` | 센서 및 입력 장치, ADC 직접 제어 |
| **HAL** | `STM32 HAL` | MCU 레지스터 제어 (ST 제공 라이브러리) |

> **설계 의도:** `fcs_math.c`나 `fcs_core.c`는 하드웨어 의존성을 최소화하여, 향후 PC 시뮬레이터나 다른 MCU로 이식할 때 코드 재사용성을 극대화했습니다.

### 2.2 Main Loop: Finite State Machine (API 기반)
`while(1)` 루프 내부의 복잡성을 제거하기 위해 **'Update - Draw'** 패턴을 적용했습니다.

```c
// Refactored Main Loop
while (1) {
    // 1. Input Phase: 모든 센서와 버튼 상태를 갱신 (Non-blocking)
    FCS_Update_Input(&fcs, &hadc1); 
    FCS_Update_Sensors(&fcs);

    // 2. Logic & UI Phase: 현재 상태(State)에 따른 로직 수행 및 렌더링
    UI_Update(&fcs);
    UI_Draw(&fcs);

    // 3. Communications: 백그라운드 명령어 처리
    FCS_Task_Serial(&fcs, &huart2);

    HAL_Delay(20); // 50Hz Tick
}
```

---

## 3. 핵심 기술 및 트러블슈팅 (Key Technologies)

### 3.1 UART 고속 통신과 Ring Buffer (Circular Buffer)
**문제 (Problem):** 
블루투스나 PC에서 `TGT:52,S,123...` 같은 긴 데이터가 고속으로 들어올 때, MCU가 화면을 그리느라 바쁘면 인터럽트를 놓쳐 데이터가 깨지는 현상(Overrun) 발생.

**해결 (Solution):** 
**Producer-Consumer 패턴**의 링 버퍼를 도입하여 수신과 처리를 분리했습니다.
- **ISR (Producer):** 인터럽트가 발생하면 즉시 바이트를 `u_buf`에 넣고 끝냅니다. (소요 시간 < 1us)
- **Task (Consumer):** 메인 루프에서 여유가 있을 때 버퍼에 쌓인 데이터를 꺼내 명령어를 완성합니다.

```c
// fcs_core.c (Concept)
void ISR_Handler() {
    buffer[head] = Uart_Read(); // 넣고
    head = (head + 1) % SIZE;   // 포인터 이동
}

void FCS_Task_Serial() {
    while (head != tail) { // 데이터가 있으면
        char ch = buffer[tail]; // 꺼내서 파싱
        tail = (tail + 1) % SIZE;
    }
}
```

### 3.2 사표(Firing Table) 보간 알고리즘
사표는 이산적인 데이터(예: 1000m, 1500m)만 존재하므로, 그 사이 거리(예: 1250m)에 대한 정확한 사각(Elevation)을 구해야 합니다.

**알고리즘:** 선형 보간법 (Linear Interpolation)
$$ Y = Y_1 + (Y_2 - Y_1) \times \frac{(X - X_1)}{(X_2 - X_1)} $$

FCS는 입력된 사거리에 맞는 구간을 **이진 탐색(Binary Search)** 혹은 순차 탐색으로 찾아낸 뒤, 위 공식을 적용하여 0.1 mil 단위의 정밀한 사각을 실시간으로 계산합니다.

### 3.3 Stack Corruption 방지
초기 개발 단계에서 `sprintf` 사용 시 버퍼 크기를 잘못 계산하여 로컬 변수(Stack)를 덮어쓰는 문제가 있었습니다.
- **대책:** 모든 문자열 버퍼를 `[64]` 바이트 이상 넉넉하게 할당하고, 가능하면 `snprintf`를 사용하여 오버플로우를 원천 차단했습니다. 또한 비즈니스 로직 내의 모든 중요 상태값은 `FCS_System_t` 구조체로 캡슐화하여 Heap/BSS 영역(Global)에서 안전하게 관리되도록 변경했습니다.

---

## 4. 결론 (Conclusion)
본 프로젝트는 단순한 기능 구현을 넘어, **'신뢰성(Reliability)'**이 생명인 군용 시스템의 특성을 소프트웨어 아키텍처 레벨에서 구현하고자 노력했습니다. 
모듈화된 구조는 향후 **블루투스 연동 앱**이나 **추가 센서 확장** 시에도 코드를 갈아엎지 않고 유연하게 대응할 수 있는 기반이 됩니다.
