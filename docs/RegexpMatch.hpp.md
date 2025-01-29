# RegexpMatch.hpp

RegexpMatch.hpp 고성능 정규식 라이브러리를 목표로 Windows, Linux 환경에 작동하고, 단순한 인터페이스를 목표로 디자인 되었습니다. 

## Todos

[ ] 전/후 검색

[ ] SIMD(Single Instruction Multiple Data)  적용

[ ] 가속을 위한 JIT 컴파일

## 주요 특징

크로스 플랫폼: Windows와 Linux 환경에서 매크로를 통한 자동 설정 동일한 작동

유니코드 지원: UTF-8, 다국어, 유니코드 문자 클래스 지원 

주요 정규식: 기본 문자, 문자 클래스 (i.e. [a-z], [0-9] etc.), 특수 문자 (\d, \w, \s), 반복사항 (*, +, ?)

## 동작 원리

RegexpMatch는 다음과 같은 방식으로 동작합니다:

1. 패턴 분석
   - 정규식 패턴을 토큰으로 분리
   - 문자 클래스와 특수 문자 해석
   - 상태 기계 구조 구축

2. 매칭 프로세스
   - 상태 기반 매칭 알고리즘 사용
   - 점진적 텍스트 스캔
   - 효율적인 백트래킹 처리

3. 최적화 기법
   - 상태 전이 테이블 최적화
   - 메모리 사용량 최소화
   - 불필요한 연산 제거

## 사용 방법

```cpp
#include "RegexpMatch.hpp"

int main() {
   RegexpMatch pattern("[a-zA-Z0-9]+");
   RegexpMatch email("[\\w\\.-]+@[\\w\\.-]+\\.\\w+");
   RegexpMatch korean("[가-힣]+");

   pattern.test("HelloWorld0");
   email.test("rampa@dc5v.com");
   korean.test("안뇽");

   return 0;
}
```


## 주의사항

 - 동일한 패턴을 여러 번 사용할 경우, RegexpMatch 객체를 재사용하세요.

 - 긴 문자열을 처리할 때는 wstring을 직접 사용하면 변환 오버헤드를 줄일 수 있습니다.

## 라이선스

상위 <a href="../LICENSE">LICENSE</a>에 종속됩니다.
