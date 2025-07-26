
#include "AbilitySystem/EffectCalc/PGEffectCalc_DamageTaken.h"

#include "PGGamePlayTags.h"
#include "AbilitySystem/PGAtrributeSet.h"
#include "Helper/Debug/PGDebugHelper.h"

/**
 * 데미지 계산에 필요한 속성들을 캡처하기 위한 구조체
 * 게임플레이 어트리뷰트 시스템에서 속성값을 안전하고 효율적으로 캡처하기 위한 정의
 */
struct FPGDamageCapture
{
	// 공격력 속성 캡처 정의 (공격자로부터)
	DECLARE_ATTRIBUTE_CAPTUREDEF(AttackPower)
	// 방어력 속성 캡처 정의 (피격자로부터)  
	DECLARE_ATTRIBUTE_CAPTUREDEF(DefensePower)
	// 데미지받기 속성 캡처 정의 (피격자에게 적용)
	DECLARE_ATTRIBUTE_CAPTUREDEF(DamageTaken)

	/**
	 * 기본 생성자 - 속성 캡처 정의 초기화
	 * Source: 이펙트를 발생시킨 액터 (공격자)
	 * Target: 이펙트를 받는 액터 (피격자)
	 * false: 스냅샷 사용 안함 (실시간 값 사용)
	 */
	FPGDamageCapture()
	{
		// 공격력은 공격자(Source)로부터 캡처
		DEFINE_ATTRIBUTE_CAPTUREDEF(UPGAtrributeSet, AttackPower, Source, false);
		// 방어력은 피격자(Target)로부터 캡처
		DEFINE_ATTRIBUTE_CAPTUREDEF(UPGAtrributeSet, DefensePower, Target, false);
		// 데미지받기는 피격자(Target)에게 적용
		DEFINE_ATTRIBUTE_CAPTUREDEF(UPGAtrributeSet, DamageTaken, Target, false);
	}
};

/**
 * 데미지 캡처 인스턴스를 반환하는 정적 함수
 * 싱글톤 패턴으로 구현하여 메모리 효율성과 성능 최적화
 * @return 데미지 캡처 구조체의 정적 인스턴스 참조
 */
static const FPGDamageCapture& GetPGDamageCapture()
{
	static FPGDamageCapture DamageCapture;
	return DamageCapture;	
}

UPGEffectCalc_DamageTaken::UPGEffectCalc_DamageTaken()
{
	// === 속성 캡처를 위한 레거시 방법 (성능상 비효율적) ===
	// 런타임에 리플렉션을 통해 속성을 찾는 방식으로 느림
	// FProperty* AttackPowerProperty= FindFieldChecked<FProperty>(
	// 	UPGAtrributeSet::StaticClass(),
	// 	GET_MEMBER_NAME_CHECKED(UPGAtrributeSet, AttackPower)
	// 	);
	//
	// FGameplayEffectAttributeCaptureDefinition AttackPowerAttributeCaptureDefinition(
	// 	AttackPowerProperty,
	// 	EGameplayEffectAttributeCaptureSource::Source,
	// 	false
	// 	);
	//
	// RelevantAttributesToCapture.Add(AttackPowerAttributeCaptureDefinition);

	// === 최적화된 속성 캡처 방법 ===
	// 컴파일 타임에 미리 정의된 캡처 정의를 사용하여 성능 향상
	// 이 클래스가 어떤 속성들을 필요로 하는지 언리얼 엔진에 알려줌
	RelevantAttributesToCapture.Add(GetPGDamageCapture().AttackPowerDef);	// 공격력 캡처 등록
	RelevantAttributesToCapture.Add(GetPGDamageCapture().DefensePowerDef);	// 방어력 캡처 등록
	RelevantAttributesToCapture.Add(GetPGDamageCapture().DamageTakenDef);	// 데미지받기 캡처 등록
}

void UPGEffectCalc_DamageTaken::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	// === 1단계: 게임플레이 이펙트 스펙 가져오기 ===
	// 현재 실행 중인 이펙트의 모든 정보를 담고 있는 스펙
	const FGameplayEffectSpec& EffectSpec = ExecutionParams.GetOwningSpec();

	// === 2단계: 속성 평가 매개변수 설정 ===
	// 태그 기반 조건부 효과나 모디파이어를 고려하기 위한 매개변수
	FAggregatorEvaluateParameters Parameters;
	Parameters.SourceTags = EffectSpec.CapturedSourceTags.GetAggregatedTags();	// 공격자의 태그들
	Parameters.TargetTags = EffectSpec.CapturedTargetTags.GetAggregatedTags();	// 피격자의 태그들

	// === 3단계: 공격자의 공격력 캡처 ===
	float SourceAttackPower = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		GetPGDamageCapture().AttackPowerDef,	// 캡처할 속성 정의
		Parameters,								// 평가 매개변수
		SourceAttackPower						// 출력될 값
	);

	// === 4단계: SetByCaller 값들 추출 ===
	// 어빌리티에서 동적으로 설정한 값들을 태그 기반으로 추출
	float BaseDamage = 0.f;					// 기본 데미지 (무기 데미지)
	int32 UsedLightAttackComboCount = 0;	// 라이트 어택 콤보 카운트
	int32 UsedHeavyAttackComboCount = 0;	// 헤비 어택 콤보 카운트

	// 이펙트 스펙에 설정된 모든 SetByCaller 값들을 순회하며 필요한 값 추출
	for (const TPair<FGameplayTag, float>& TagMagniture : EffectSpec.SetByCallerTagMagnitudes)
	{
		// 기본 데미지 값 추출
		if (TagMagniture.Key.MatchesTagExact(PGGamePlayTags::Shared_SetByCaller_BaseDamage))
		{
			BaseDamage = TagMagniture.Value;
		}
		// 라이트 어택 콤보 카운트 추출
		if (TagMagniture.Key.MatchesTagExact(PGGamePlayTags::Player_SetByCaller_AttackType_Light))
		{
			UsedLightAttackComboCount = TagMagniture.Value;
		}
		// 헤비 어택 콤보 카운트 추출
		if (TagMagniture.Key.MatchesTagExact(PGGamePlayTags::Player_SetByCaller_AttackType_Heavy))
		{
			UsedHeavyAttackComboCount = TagMagniture.Value;
		}
	}
	
	// === 5단계: 피격자의 방어력 캡처 ===
	float TargetDefensePower = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		GetPGDamageCapture().DefensePowerDef,	// 캡처할 속성 정의
		Parameters,								// 평가 매개변수  
		TargetDefensePower						// 출력될 값
	);
	
	// === 6단계: 콤보 시스템에 따른 데미지 스케일링 ===
	
	// 라이트 어택 콤보 데미지 증가 계산
	// 공식: 1 + (콤보카운트 - 1) * 0.05 (콤보당 5% 증가)
	if (UsedLightAttackComboCount != 0)
	{
		const float DamageIncreasePercentLight = (UsedLightAttackComboCount - 1) * 0.05f + 1.f;
		BaseDamage *= DamageIncreasePercentLight;
	}

	// 헤비 어택 콤보 데미지 증가 계산  
	// 공식: 1 + (콤보카운트 - 1) * 0.15 (콤보당 15% 증가)
	if (UsedHeavyAttackComboCount != 0)
	{
		const float DamageIncreasePercentHeavy = (UsedHeavyAttackComboCount - 1) * 0.15f + 1.f;
		BaseDamage *= DamageIncreasePercentHeavy;
	}

	// === 7단계: 최종 데미지 계산 ===
	// 최종 데미지 = 기본데미지 * 콤보배율 * (공격력 / 방어력)
	// 방어력이 높을수록 받는 데미지가 줄어드는 구조
	const float FinalDamage = BaseDamage * SourceAttackPower / TargetDefensePower;
	PG_Debug::Print(TEXT("FinalDamage"), FinalDamage);
	
	// === 8단계: 결과 출력 ===
	// 계산된 데미지가 0보다 클 때만 속성에 적용
	if (FinalDamage > 0.f)
	{
		// DamageTaken 속성에 계산된 최종 데미지 값을 Override 방식으로 설정
		// Override: 기존 값을 무시하고 새 값으로 덮어씀
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				GetPGDamageCapture().DamageTakenProperty,	// 수정할 속성
				EGameplayModOp::Override,					// 수정 방식 (덮어쓰기)
				FinalDamage									// 적용할 값
			));
	}
}
