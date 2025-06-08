# PGBlueprintUtil 에디터 기능 - Phase 1 구현 완료

## 📋 구현 상태

### ✅ Phase 1 완료 (기본 기능)
- [x] 에디터 메뉴 통합
- [x] 기본 검색 기능
- [x] 간단한 결과 표시

## 🎯 구현된 기능

### 1. 에디터 메뉴 통합
**메뉴 위치**: `에디터 메뉴바 → 블루프린트 → 부모 클래스별 블루프린트 검색`
- 블루프린트 메뉴에 새로운 섹션 "블루프린트 유틸리티" 추가
- 단축키 지원: `Ctrl + Shift + B`
- 툴바에도 검색 버튼 추가

### 2. 검색 도구 (`UPGBlueprintSearchTool`)
- **AssetRegistry 기반 검색**: 프로젝트 내 모든 블루프린트 에셋 스캔
- **부모 클래스 필터링**: 선택된 부모 클래스를 상속받는 블루프린트만 추출
- **검색 범위 설정**: 프로젝트만/플러그인 포함/엔진 포함
- **비동기 검색**: UI 블록킹 없이 백그라운드에서 검색 수행
- **캐싱 시스템**: 동일한 검색 조건에 대한 결과 캐싱

### 3. 사용자 인터페이스 (`SPGBlueprintSearchWidget`)
- **클래스 선택기**: 내장 ClassViewer를 통한 직관적인 클래스 선택
- **검색 옵션**: 추상 클래스 포함, 정확한 일치만 등 세부 옵션
- **결과 리스트**: 다중 컬럼 테이블로 검색 결과 표시
- **실시간 필터링**: 검색 결과 내에서 텍스트 기반 추가 필터링
- **상호작용**: 더블클릭으로 블루프린트 열기, 우클릭 컨텍스트 메뉴

## 🏗️ 구현된 아키텍처

### 모듈 구조
```
PGBlueprintUtil/
├── Public/
│   ├── PGBlueprintUtil.h                    # 메인 모듈 헤더
│   ├── PGBlueprintUtilCommands.h            # 에디터 명령어 정의
│   ├── PGBlueprintSearchTypes.h             # 검색 관련 데이터 타입
│   ├── PGBlueprintSearchTool.h              # 검색 엔진
│   └── PGBlueprintSearchWidget.h            # UI 위젯
└── Private/
    ├── PGBlueprintUtil.cpp                  # 메인 모듈 구현
    ├── PGBlueprintUtilCommands.cpp          # 명령어 구현
    ├── PGBlueprintSearchTypes.cpp           # 데이터 타입 구현
    ├── PGBlueprintSearchTool.cpp            # 검색 엔진 구현
    └── PGBlueprintSearchWidget.cpp          # UI 위젯 구현
```

## 🎮 사용법

### 1. 기본 사용법
1. 에디터에서 `블루프린트` 메뉴 클릭
2. `부모 클래스별 블루프린트 검색` 선택
3. 검색할 부모 클래스 선택 (예: ACharacter)
4. 검색 옵션 설정
5. `검색` 버튼 클릭
6. 결과 리스트에서 원하는 블루프린트 더블클릭

### 2. 고급 기능
- **필터링**: 결과 창 상단의 검색 박스로 실시간 필터링
- **컨텍스트 메뉴**: 결과 아이템 우클릭으로 추가 옵션
- **키보드 단축키**: `Ctrl + Shift + B`로 빠른 실행

## 📝 빌드 및 배포

### 빌드 요구사항
- 언리얼 엔진 5.4 이상
- C++20 지원 컴파일러
- 에디터 전용 기능이므로 Development Editor 구성 필요

### 빌드 확인사항
1. 프로젝트 파일 재생성 필요
2. Development Editor 구성으로 빌드
3. 에디터 실행 후 블루프린트 메뉴에서 기능 확인

---

**Phase 1 구현 완료** ✅
