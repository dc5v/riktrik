# TictacDB

## 1. 시스템 개요

### 1.1 목적
TictacDB는 산업용 설비의 대규모 센서 데이터를 처리하기 위한 특화된 시계열 데이터베이스입니다. 
일반적인 데이터베이스와 달리, 센서에서 발생하는 시계열 데이터를 초고속으로 저장하고 검색하는 데 최적화되어 있습니다.
그리고 개인적인 프로젝트라 좆같아서 소스 코드를 공개합니다.


### 1.2 주요 요구사항
- **초당 20만개 데이터 처리**: 대규모 공장이나 설비에서 발생하는 센서 데이터를 실시간으로 처리
- **10년 이상 데이터 보관**: 설비의 장기적인 상태 분석과 예측을 위한 히스토리 데이터 유지
- **데이터 유실 방지**: 공장 설비의 중요 데이터이므로 절대로 유실되면 안 됨
- **고성능 검색**: 오래된 데이터라도 빠르게 검색할 수 있어야 함
- **안정적인 샤딩**: 대용량 데이터를 여러 서버에 분산 저장하여 부하 분산

### 1.3 데이터 구조
```cpp
struct Record {
    uint32_t id;          // 4바이트: 데이터의 고유 식별자
    char tag[256];        // 256바이트: 식별자 (예: "PRESSURE_010")
    double value;         // 8바이트: 센서 측정값
    uint32_t status;      // 4바이트: 데이터 상태 코드
    uint64_t sensored_at; // 8바이트: 센서 측정 시간 Epoch MS
    uint64_t queued_at;   // 8바이트: 큐에 입력된 시간 Epoch MS
    uint64_t stored_at;   // 8바이트: 실제 저장된 시간 Epoch MS
    uint8_t is_deleted;   // 1바이트: 삭제 표시
    uint8_t padding[3];   // 3바이트: 메모리 정렬
};  // 총 292바이트
```

## 2. 시스템 아키텍처

### 2.1 물리적 구성
시스템은 세 가지 주요 컴포넌트로 구성됩니다:

1. **수집 서버**
   - 센서로부터 데이터를 직접 수집
   - OPC-UA, OPC-DA 등 산업용 프로토콜 지원
   - 초기 데이터 검증 수행

2. **매니저 서버**
   - 이중화 구성으로 무중단 운영
   - 전체 시스템의 상태 모니터링
   - 샤드 간 부하 분산 관리
   - 데이터 복제 및 백업 관리

3. **샤드 서버**
   - 실제 데이터 저장 담당
   - 각각 독립된 저장소 사용
   - 특정 태그 그룹의 데이터만 처리

### 2.2 저장소 구조

파일 시스템 구조는 다음과 같이 계층적으로 구성됩니다:

```
data                 # 최상위 데이터 디렉토리
|-- 2024             # 연도별 구분
    |- 01            # 월별 구분
       |- 01         # 일별 구분
          |- tag1.db # 태그별 데이터 파일
          |- tag2.db
        ...
```
