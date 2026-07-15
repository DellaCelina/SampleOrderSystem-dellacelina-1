# S-Semi — 반도체 시료 생산주문관리 시스템

콘솔 기반 반도체 시료(Sample) 생산/주문 관리 시스템입니다. 주문 접수부터 승인/거절, 재고 부족 시
자동 생산 큐 등록, 생산 완료 후 출고까지의 흐름을 콘솔에서 처리합니다. 전체 기능 명세는
[docs/REQUIREMENT.md](docs/REQUIREMENT.md), 설계는 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)를
참고하세요.

## 빌드

Visual Studio C++ 프로젝트(`SampleOrderSystem.slnx`)이며 CMake는 사용하지 않습니다.

**Visual Studio에서:** `SampleOrderSystem.slnx`를 열고 `SampleOrderSystem` 프로젝트를 Debug 또는
Release, x64(또는 Win32) 구성으로 빌드합니다.

**명령줄(git-bash)에서:**

```bash
export MSYS2_ARG_CONV_EXCL="*"
"/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" \
  SampleOrderSystem/SampleOrderSystem.vcxproj \
  /p:Configuration=Debug /p:Platform=x64 /m:1 /v:minimal
```

빌드 시 저장소 루트의 `data/`, `schema/` 폴더가 빌드 출력 폴더(`$(OutDir)`)로 자동 복사되므로,
실행 파일은 어느 위치에서 실행하든 자기 자신의 디렉터리를 기준으로 데이터/스키마 파일을 찾습니다.

## 실행

빌드 후 실행 파일은 `SampleOrderSystem/x64/Debug/SampleOrderSystem.exe`(구성에 따라 경로가
달라짐)에 생성됩니다.

```bash
./SampleOrderSystem/x64/Debug/SampleOrderSystem.exe
```

### 대화형(인터랙티브) 모드

인자 없이 실행하면 메인 메뉴가 표시되고, 번호를 입력해 각 기능으로 진입합니다.

```
1. 시료 등록
2. 시료 목록 조회
3. 시료 검색
4. 주문 관리        (접수 / 승인 대기 목록 / 승인 / 거절 / 출고 처리)
5. 모니터링 요약
6. 생산 라인 조회
7. 데이터 모니터
8. 더미 데이터 생성
0. 종료
```

숫자가 아닌 입력이나 목록에 없는 번호를 입력하면 오류 메시지를 보여주고 메뉴를 다시 표시합니다.
입력이 파이프로 연결되어 있다가 끝(EOF)에 도달하면 프로그램은 정상 종료(exit code 0)합니다.

### CLI 플래그 (인터랙티브 메뉴 없이 즉시 실행 후 종료)

| 플래그 | 동작 |
|---|---|
| (없음) | 대화형 메인 메뉴 진입 |
| `--dummy-data` 또는 `--dummy-data=N` | 더미 시료/주문 데이터를 N개(기본값 20) 생성하고 종료 |
| `--data-monitor` | 정산(lazy settlement) 후 현재 데이터 상태(시료/주문/생산 큐)를 한 번 출력하고 종료 |
| `--help` / `-h` | 사용법 한 줄 출력 후 종료 |

`--dummy-data`와 `--data-monitor`를 동시에 지정하거나 인식할 수 없는 플래그를 넘기면 오류를 출력하고
0이 아닌 코드로 종료합니다.

```bash
# 더미 데이터 5개(시료/주문 각각) 생성
./SampleOrderSystem/x64/Debug/SampleOrderSystem.exe --dummy-data=5

# 현재 데이터 상태 한 번 확인
./SampleOrderSystem/x64/Debug/SampleOrderSystem.exe --data-monitor
```

### 데이터 파일

`data/samples.json`, `data/orders.json`, `data/production_queue.json`에 저장되며, 실행 파일과 같은
폴더의 `data/` 하위에 위치합니다. 최초 실행 시 파일이 없으면 빈 테이블로 취급하고, 파일은 있으나
JSON 형식이 손상된 경우에는 오류를 출력하고 종료합니다(부분적으로 읽고 진행하지 않음). 스키마는
`schema/*.schema.json`에 정의되어 있습니다.

## 테스트

GoogleTest/GoogleMock 기반이며, 별도 프로젝트(`SampleOrderSystemTests.vcxproj`)에서
`SampleOrderSystem`의 비-UI 소스를 다시 컴파일해 테스트합니다.

```bash
export MSYS2_ARG_CONV_EXCL="*"
"/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" \
  SampleOrderSystemTests/SampleOrderSystemTests.vcxproj \
  /p:Configuration=Debug /p:Platform=x64 /m:1 /v:minimal

./SampleOrderSystemTests/x64/Debug/SampleOrderSystemTests.exe
# 특정 스위트/테스트만 실행
./SampleOrderSystemTests/x64/Debug/SampleOrderSystemTests.exe --gtest_filter=OrderServiceTest.*
```

빌드/테스트 명령의 더 자세한 배경(왜 `MSYS2_ARG_CONV_EXCL`이 필요한지, 병렬 빌드 시 주의사항 등)은
[CLAUDE.md](CLAUDE.md)를 참고하세요.
