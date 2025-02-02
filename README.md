# riktrik

<img src="./docs/assets/logo.svg" width="200" alt="logo">

[[_TOC_]]

`RIKTRIK` derived from the Tibetan word "རིགས་གྲིག་". This term is used in Tibetan Buddhism and means 'order' or 'systematic arrangement'

_'`RIKTRIK`은 티벳어 리크트리크("རིགས་གྲིག་")입니다. 이 의미는 티베트 불교에서 사용되는 용어로, "순서" 또는 "체계적인 배열"을 의미합니다.'_

## this DBMS's design goal 

`RIKTRIK` is time-series DBMS designed to efficiently compress and store only the necessary parts of sensor or equipment-sensed data, enabling fast retrieval and search.

_'RIKTRIK은 센서 또는 장비에서 감지된 데이터의 필요한 부분만 효율적으로 압축 및 저장하도록 설계된 시계열 DBMS로, 빠른 검색과 조회를 가능하도록 디자인되었습니다.'_

---

## Structure basis

`RIKTRIK` uses the custom type `uint24_t` when designing memory, file structure. This type is defined in [AdvancedType.hpp](./lib/types/AdvancedType.hpp).

_'`RIKTRIK`은 메모리 및 파일 구조를 설계할 때 커스텀 타입 `uint24_t`를 사용합니다. 이 타입은 [AdvancedType.hpp](./lib/types/AdvancedType.hpp)에 정의되어 있습니다. 메모리최적화를 위해 쉬프트연산시 약간의 어셈블러를 사용했습니다 (효과가 있을지는 아직 의문이지만..)'_

### Memory DB

*NID*(Node ID) represents the unique ID of each device (equipment) sensor.
This data is stored in a memory database for fast indexing.

_'NID(Node ID)는 각 장비(설비) 센서의 고유 ID를 나타냅니다. 이 데이터는 빠른 인덱싱을 위해 메모리 데이터베이스에 저장됩니다.'_

|    Name    | Type      | Size(bytes) | Range                          |
| :--------: | --------- | ----------: | ------------------------------ |
|   *NID*    | uint24_t  |           3 | 0 - 16,777,215                 |
|    NAME    | char[255] |         255 | CHAR 255                       |
|   VALUE    | int32_t   |           4 | -9999.00000 - 9999.00000       |
|   STATUS   | uint8_t   |           1 | 0 - 255                        |
| UPDATED_AT | uint64_t  |           8 | 0 - 18,446,744,073,709,551,615 |


> i.e. 
> 271 bytes / 1 NID 
> 
> e.g. 
> If using 200,000 NIDs with the [KvStore.hpp](./docs/KvStore.hpp.md) library, approximately 206.8MB of memory will be used.
>
> _즉, [KvStore.hpp](./docs/KvStore.hpp.md)라이브러리를 사용해 메모리 DB를 구성할경우, NID개당 271 bytes의 메모라를 사용하고_
> 
> _예를들어, 20만개의 NID가 있다면 206.8MB 정도의 메모리를 사용합니다._
### Log Data

Log records will only when `STATUS` or `VALUE` changes using a `Memory DB`. 
And log files are sharding by [year, month, NID, day] to reduce overhead and unnecessary data storage, 
facilitating data searches and simplifying the archiving of Hot and Cold data.

_로그의 기록은 `Memory DB`를 사용하여 `STATUS` 또는 `VALUE`가 변경될 때만 저장합니다. 또한, 로그 파일은 [년, 월, NID, 일] 단위로 샤딩되어 오버헤드를 줄이고 필요한 데이만터 저장하고, 데이터 검색을 용이하게 하도록 디자인했으며, Hot|Cold 데이터의 아카이빙을 간소화합니다._

|  Name  | Type     | Size | Range                    |
| :----: | -------- | ---: | ------------------------ |
| VALUE  | int32_t  |    4 | -9999.00000 - 9999.00000 |
| STATUS | uint8_t  |    1 | 0 - 255                  |
|  TIME  | uint24_t |    3 | 0 - 8,640,099            |

```
[filenameㅡㄷ rule]

data
├── 2025
│   └── 01
│       ├── [NID]-01.db
│       └── [NID]-02.db
...
```

> `i.e.`
> 8 bytes/sec.
> 
> If the value changes every time, 691,200 bytes/day.
> 
> `e.g.`
> 
> If the average change rate is 30% (entropy ≈ 0.3)
> 
> 1 NID ≈ 207,360 bytes/day, **200,000 NID ≈ 41.5 GB/day**

