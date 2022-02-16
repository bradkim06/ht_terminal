## 테스트코드 변경 이력

### test_METER_clearIntervalData (2022-02-15)

Uplink 성공시 검침데이터를 주기보고 간격만큼 삭제 함수

### METER_clearStoredData (2022-02-15)

검침 데이터 삭제 함수

### MODEM_qaData 추가 (2022-02-11)

LGU+ 품질리포트 Msg 생성 함수

### parseQGMR 추가 (2022-02-11)

모뎀 F/W Version Read 함수

### METER_addStoredData 추가 (2022-02-09)

검침데이터 저장 처리 알고리즘

Config dataSkipMode 추가 (2022-02-16)

- 수정사항
  - RTC_calcSecDiff()에서 struct tm(local variable) 초기화 추가항목
    - local variable 미초기화로 잘못된 result 나오는 Case 발견(수정)

```c
long RTC_calcSecDiff(Date_t *prev, Date_t *next)
{
	struct tm prev_time; --> struct tm prev_time = {0};
}
```

- METER_addStoredData() Refactoring
  - nData == Max일때 검침데이터 일부 메모리 삭제 안되는 부분 수정 (2022-02-16)

##### Before

```
================ Test Case(5) : saveInterval 2->4, nData Max ==================
Date:2022-2-9,12:39:0 Stored Date:2022-2-9,10:39:0 nData(24)
Input stored.unit[ 0] : 16 02 09 0A 27 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Input stored.unit[ 1] : 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02
Input stored.unit[ 2] : 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03
Input stored.unit[ 3] : 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04
Input stored.unit[ 4] : 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05
Input stored.unit[ 5] : 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06
Input stored.unit[ 6] : 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07
Input stored.unit[ 7] : 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08
Input stored.unit[ 8] : 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09
Input stored.unit[ 9] : 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A
Input stored.unit[10] : 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B
Input stored.unit[11] : 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C
Input stored.unit[12] : 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D
Input stored.unit[13] : 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E
Input stored.unit[14] : 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F
Input stored.unit[15] : 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10
Input stored.unit[16] : 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11
Input stored.unit[17] : 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12
Input stored.unit[18] : 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13
Input stored.unit[19] : 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14
Input stored.unit[20] : 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15
Input stored.unit[21] : 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16
Input stored.unit[22] : 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17
Input stored.unit[23] : 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18

Output stored.unit[ 0] : 16 02 09 0C 27 00 01 AA AA AA AA AA AA AA AA AA AA AA AA AA AA
Output stored.unit[ 1] : 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02
Output stored.unit[ 2] : 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04
Output stored.unit[ 3] : 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06
Output stored.unit[ 4] : 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08
Output stored.unit[ 5] : 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A
Output stored.unit[ 6] : 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C
Output stored.unit[ 7] : 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E
Output stored.unit[ 8] : 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10
Output stored.unit[ 9] : 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12
Output stored.unit[10] : 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14
Output stored.unit[11] : 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16
Output stored.unit[12] : 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18
Output stored.unit[13] : 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E
Output stored.unit[14] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Output stored.unit[15] : 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10
Output stored.unit[16] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Output stored.unit[17] : 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12
Output stored.unit[18] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Output stored.unit[19] : 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14
Output stored.unit[20] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Output stored.unit[21] : 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16
Output stored.unit[22] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Output stored.unit[23] : 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18
```

#### After

```
================ Test Case(5) : saveInterval 2->4, nData Max ==================
Date:2022-2-9,12:39:0 Stored Date:2022-2-9,10:39:0 nData(24)
Input Meter Data
stored.unit[ 0] : 16 02 09 0A 27 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00
stored.unit[ 1] : 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02
stored.unit[ 2] : 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03
stored.unit[ 3] : 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04
stored.unit[ 4] : 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05 05
stored.unit[ 5] : 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06
stored.unit[ 6] : 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07 07
stored.unit[ 7] : 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08
stored.unit[ 8] : 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09 09
stored.unit[ 9] : 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A
stored.unit[10] : 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B 0B
stored.unit[11] : 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C
stored.unit[12] : 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D
stored.unit[13] : 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E
stored.unit[14] : 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F 0F
stored.unit[15] : 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10
stored.unit[16] : 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11 11
stored.unit[17] : 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12
stored.unit[18] : 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13 13
stored.unit[19] : 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14
stored.unit[20] : 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15
stored.unit[21] : 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16
stored.unit[22] : 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17
stored.unit[23] : 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18
Output Meter Data [After METER_addStoredData()]
stored.unit[ 0] : 16 02 09 0C 27 00 01 AA AA AA AA AA AA AA AA AA AA AA AA AA AA
stored.unit[ 1] : 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02
stored.unit[ 2] : 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04
stored.unit[ 3] : 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06 06
stored.unit[ 4] : 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08 08
stored.unit[ 5] : 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A 0A
stored.unit[ 6] : 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C 0C
stored.unit[ 7] : 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E 0E
stored.unit[ 8] : 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10
stored.unit[ 9] : 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12
stored.unit[10] : 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14 14
stored.unit[11] : 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16
stored.unit[12] : 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18 18
stored.unit[13] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
stored.unit[14] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
stored.unit[15] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
stored.unit[16] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
stored.unit[17] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
stored.unit[18] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
stored.unit[19] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
stored.unit[20] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
stored.unit[21] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
stored.unit[22] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
stored.unit[23] : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

### Gcov(Test Code Coverage Tool) 추가 (2022-02-08)

테스트 결과 html파일로 생성

### distributingReportTime 추가 (2022-02-07)

보고시간 분산 알고리즘 함수

- FLASH_readConfigInfo() 함수에 통합되어 있던 코드 분할 -> distributingReportTime()

### checkInterval 추가 (2022-01-28)

단말기의 보고시간 여부 Check 함수

### Ceedling 추가 (2022-01-25)

펌웨어 품질 향상을 위해 단위 유닛 테스트 기능 추가

- Ceedling 단위 유닛 테스트 Tool 기능 추가
- parseCEREG 테스트 추가
