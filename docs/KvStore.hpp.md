# KvStore.hpp

이 라이브러리는 Key-Value Store로 기본기능에 집중하여 단순하게 사용할 수 있으며
BloomFilter와 CuckooFilter를 활용한 효율적인 검색, SIMD 연산을 통한 벡터화, 락프리 캐시와 같은 최신 기술을 적용한 고성능 인메모리 저장소를 목표로 디자인되었습니다.

## Todos

- [ ] LSM Tree

- [ ] HyperLogLog Filter

## 주요특징

크로스 플랫폼: Windows와 Linux 환경에서 매크로를 통한 자동 설정 동일한 작동

YAML: YAML 형식의 데이터를 사용해 shared_mutex를 통한 읽기-쓰기 동기화 데이터 저장 및 로드

메모리 최적화: 문자열 풀링과 Boost Pool Allocator를 통한 메모리 사용량 최소화

SIMD 연산 지원: AVX2를 활용한 벡터화된 검색 연산으로 대량 데이터 처리 성능 향상

병렬 처리: OpenMP를 활용한 대용량 데이터의 병렬 처리 지원

BloomFilter: 메모리 효율적인 확률적 자료구조로, 빠른 negative 검색 제공

CuckooFilter: 낮은 위양성률과 삭제 연산을 지원하는 고급 필터링


## 사용 방법

```cpp
#include "KvStore.hpp"

KvStore store("data.yml");
store.setFilter(KvFilterType::BLOOM); // KvFilterType::CUCKOO

store.push("name", "직류오볼트");
store.push("acdc", "dc");
store.push("v", 5);

if (auto data = store.key("name")) 
{
  if (auto name = data->getString()) 
  {
    cout << "name: " << *name << std::endl;
  }
}
```

## 의존성
- Boost
- Intel TBB
- yaml-cpp
- OpenMP

