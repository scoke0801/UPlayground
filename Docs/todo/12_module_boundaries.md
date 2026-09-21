# 12. 모듈 경계와 수명 안전성 정리

우선순위 P2 / 상태 필요한 경계 정리·검증 반영 / 선행 01, 11의 회귀 기준 / 규모 중

## 현재 근거

- Build.cs에 PGAbilitySystem→PGActor/PGAI, PGActor→PGUI, PGData→PGAbilitySystem 순환 참조 선언이 존재한다. 일부는 5.8 마이그레이션 보존 목적 주석이 있다.
- PGShared는 PGAbilitySystem의 include 모듈을 참조하고 AIModule 의존성을 중복 기재한다. PGAbilitySystem의 PGData와 PGUI의 PGMessage도 중복 기재되어 있다.
- `PGStageManager.cpp`가 PGUI 구체 위젯을 직접 열고, `PGAbilitySystemComponent`가 UI 클릭 메시지를 구독한다.
- Data/Message/Floater 매니저에 static WeakThis/Get 패턴이 있다. 복수 PIE 월드와 종료 순서의 안전성은 검증 대상이다.

## 목표와 단계

기존 8개 모듈을 유지하며 기능을 고칠 때 필요한 의존성부터 줄인다. 순환 참조 설정만 먼저 삭제하거나 전체 구조를 일괄 교체하지 않는다.

1. 실제 include와 public 헤더 노출 타입을 조사하여 의존성 표를 만든다. 중복 항목과 불필요 의존성을 검증 후 제거한다.
2. PGShared에는 식별자·값 타입·작은 인터페이스만 둔다. 구체 Actor/UI/Ability 구현을 공통 타입으로 끌어올리지 않는다.
3. 진행 상태→PGMessage 알림→PGUI 표시, UI 선택→게임플레이 요청의 계약을 정의한다. 표현 계층이 진행/보상 지급을 소유하지 않는다.
4. 구체 Actor 참조가 필요한 GAS/AI 경로는 작은 인터페이스 또는 책임 이동으로 줄인다. Data의 어빌리티 참조는 필요에 따라 엔진 기본 타입/소프트 참조로 표현한다.
5. 매니저 접근에 World/GameInstance/LocalPlayer 문맥을 명확히 한다. 구독·타이머는 소유자의 EndPlay/Deinitialize에서 정리하고 객체 파괴 단계에만 의존하지 않는다.
6. 연결을 끊은 모듈 쌍부터 순환 참조 예외를 제거하고 빌드·패키지·기존 BP 로드를 확인한다.

## 수용 기준

- 제거한 예외마다 실제 양방향 참조가 해소되었고 공개 헤더의 의존성이 올바르다.
- UI를 표시하지 않아도 스테이지/보상 로직이 검증되며 UI 중복 생성으로 이벤트가 중복 처리되지 않는다.
- 복수 PIE 인스턴스 또는 반복 시작/종료에서 다른 인스턴스에 메시지·데이터가 전달되지 않는다.
- 기존 에셋이 정상 로드된다. PG 명명 교정이 필요하면 리다이렉트·재저장·참조 검사까지 별도 변경으로 처리한다.

디버그: 구독 수·월드 식별자·종료 시 잔여 작업 표시. 후순위: 신규 범용 프레임워크, 전면 Public/Private 이동, 순수 미관 목적의 대량 이름 변경.

## 2026-09-19 구현

PGData→PGAbilitySystem 예외와 PGShared→PGAbilitySystem include를 제거했다. 공개 헤더에 노출하는 모듈은 public 의존성으로 선언했다. StageManager/보상 위젯을 메시지와 지급 콜백으로 분리하고 월드 문맥을 사용하는 메시지/UI/플로터 접근을 추가했다. 실제 참조가 남은 Actor/GAS/AI/UI 순환 예외는 유지한다. 의존성 목록과 검증 범위는 [3·4단계 보고서](Stage34_Implementation_Report.md)를 따른다.
