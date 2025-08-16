// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGShared/Shared/Message/Base/PGMessageEventDataBase.h"

#include "PGMessageManager.generated.h"

// 컴파일 타임 자동 타입 ID 생성
template<typename T>
struct TEnumTypeID
{
    static inline uint32 GetID(uint8 value)
    {
        if (const UEnum* EnumPtr = StaticEnum<T>())
        {
            // C++ 식별자 이름 사용 - DLL 경계에서 안전
            FString EnumName = EnumPtr->GetNameStringByValue(value);
            
            return GetTypeHash(EnumName);
        }

        checkf(nullptr, TEXT("T Must be UENUM"));
        
        return 0;
    }
};

// 통합 키 (메모리 효율적)
struct FPGEnumKey
{
    uint8 Value;      // enum 값 (uint8로 제한)
    uint32 TypeHash;  // 타입 해시

    FPGEnumKey() : Value(0),TypeHash(0) {}
    
    template<typename EnumType>
    FPGEnumKey(EnumType InEnum)
        : Value(static_cast<uint8>(InEnum))
        , TypeHash(TEnumTypeID<EnumType>::GetID(Value))
    {
        static_assert(sizeof(EnumType) == sizeof(uint8), "Enum must be uint8 based");
    }

    bool operator==(const FPGEnumKey& Other) const
    {
        return TypeHash == Other.TypeHash && Value == Other.Value;
    }

    // friend 함수로 변경하여 ADL 제한
    friend uint32 GetTypeHash(const FPGEnumKey& Key)
    {
        return HashCombine(Key.TypeHash, ::GetTypeHash(Key.Value));
    }
};


/**
 * 메시지 매니저 - FPGEnumKey를 키로 사용하여 델리게이트를 관리
 */
UCLASS()
class PGMESSAGE_API UPGMessageManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

    static TWeakObjectPtr<UPGMessageManager> WeakThis;
    
private:
    // Key-Delegate 맵
    TMap<FPGEnumKey, TSharedPtr<FPGMessageDelegate>> MessageDelegates;
    
public:
	static UPGMessageManager* Get();
    
public:
    // USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * 델리게이터 등록
     * @param Key 메시지 키
     * @param Callback 실행할 콜백 함수
     * @return 델리게이트 핸들
     */
    template<typename EnumType>
    FDelegateHandle RegisterDelegate(EnumType Enum, TDelegate<void(const IPGEventData*)> Callback)
    {
        FPGEnumKey Key(Enum);
        
        if (!MessageDelegates.Contains(Key))
        {
            MessageDelegates.Add(Key, MakeShareable(new FPGMessageDelegate()));
        }
        
        return MessageDelegates[Key]->AddLambda(Callback);
    }
    
    /**
     * UObject 멤버 함수 델리게이터 등록
     * @param Key 메시지 키
     * @param UserObject 대상 오브젝트
     * @param Func 멤버 함수 포인터
     * @return 델리게이트 핸들
     */
    template<typename EnumType, typename UserClass>
    FDelegateHandle RegisterDelegate(EnumType Enum, UserClass* UserObject, void(UserClass::*Func)(const IPGEventData*))
    {
        FPGEnumKey Key(Enum);
        
        if (!MessageDelegates.Contains(Key))
        {
            MessageDelegates.Add(Key, MakeShareable(new FPGMessageDelegate()));
        }
        
        return MessageDelegates[Key]->AddUObject(UserObject, Func);
    }
    
    /**
     * 델리게이터 등록 해제
     * @param Key 메시지 키
     * @param Handle 삭제할 델리게이트 핸들
     */
    template<typename EnumType>
    void UnregisterDelegate(EnumType Enum, FDelegateHandle Handle)
    {
        FPGEnumKey Key(Enum);
        
        if (MessageDelegates.Contains(Key))
        {
            MessageDelegates[Key]->Remove(Handle);
        }
    }
    
    /**
     * 메시지 전송
     * @param Key 메시지 키
     * @param Data 전송할 데이터 (nullable)
     */
    template<typename EnumType>
    void SendMessage(EnumType Enum, const IPGEventData* Data = nullptr)
    {
        FPGEnumKey Key(Enum);
        
        if (MessageDelegates.Contains(Key) && MessageDelegates[Key].IsValid())
        {
            MessageDelegates[Key]->Broadcast(Data);
        }
    }

    /**
     * 특정 키의 모든 델리게이터 제거
     * @param Key 메시지 키
     */
    template<typename EnumType>
    void ClearDelegates(EnumType Enum)
    {
        FPGEnumKey Key(Enum);
        
        if (MessageDelegates.Contains(Key))
        {
            MessageDelegates[Key]->Clear();
            MessageDelegates.Remove(Key);
        }
    }
    
    /**
     * 모든 델리게이터 제거
     */
    void ClearAllDelegates();
};

#define PGMessage() UPGMessageManager::Get()