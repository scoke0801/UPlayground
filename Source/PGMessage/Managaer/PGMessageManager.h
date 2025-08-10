// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PGMessageManager.generated.h"

/*
 구독
 Subscribe(AnyEnumType::AnyValue, listener, callback);
 
 // 람다 구독
 SubscribeLambda(AnyEnumType::AnyValue, [](auto type, auto data){});
 
 // 발송
 SendMessage(AnyEnumType::AnyValue, data);
 
 // 조건부 발송
 SendMessageIf(AnyEnumType::AnyValue, data, predicate);
 
 // 구독 해제
 Unsubscribe(AnyEnumType::AnyValue, listener); 
*/

// 이벤트 데이터 인터페이스
class PGMESSAGE_API IPGEventData
{
public:
    virtual ~IPGEventData() = default;
    virtual FString ToString() const = 0;
};

// 이벤트 키 구조체 (타입 + 값으로 유니크 키 생성)
USTRUCT()
struct PGMESSAGE_API FPGEventKey
{
    GENERATED_BODY()

    FString TypeName;
    int32 EventValue;

    FPGEventKey()
    {
        TypeName = TEXT("");
        EventValue = 0;
    }

    FPGEventKey(const FString& InTypeName, int32 InEventValue)
        : TypeName(InTypeName), EventValue(InEventValue)
    {
    }

    // 비교 연산자
    bool operator==(const FPGEventKey& Other) const
    {
        return TypeName == Other.TypeName && EventValue == Other.EventValue;
    }

    // 해시 함수를 위한 연산자
    friend uint32 GetTypeHash(const FPGEventKey& Key)
    {
        return HashCombine(GetTypeHash(Key.TypeName), GetTypeHash(Key.EventValue));
    }

    FString ToString() const
    {
        return FString::Printf(TEXT("%s::%d"), *TypeName, EventValue);
    }
};

// 통합 이벤트 델리게이트
DECLARE_MULTICAST_DELEGATE_TwoParams(FUniversalEventDelegate, FPGEventKey, UObject*);

/**
 * 단순화된 통합 이벤트 서브시스템
 */
UCLASS()
class PGMESSAGE_API UPGMessageManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

    static TWeakObjectPtr<UPGMessageManager> WeakThis;
    
private:
    // 모든 이벤트를 저장하는 하나의 맵
    TMap<FPGEventKey, FUniversalEventDelegate> EventDelegates;
    
    // 리스너별 구독 이벤트 추적
    TMap<UObject*, TArray<FPGEventKey>> ListenerSubscriptions;

public:
	static UPGMessageManager* Get();
    
public:
    // USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

public:
    // 템플릿 기반 구독 (내부적으로 FEventKey로 변환)
    template<typename TEnum, typename TCallable>
    void Subscribe(TEnum EventType, UObject* Listener, TCallable Callback)
    {
        FPGEventKey Key = MakeEventKey(EventType);
        
        if (!Listener)
        {
            UE_LOG(LogTemp, Warning, TEXT("Listener is null for event: %s"), *Key.ToString());
            return;
        }

        // 이벤트 델리게이트가 없으면 생성
        if (!EventDelegates.Contains(Key))
        {
            EventDelegates.Add(Key, FUniversalEventDelegate());
        }

        // 델리게이트에 콜백 추가
        EventDelegates[Key].AddUObject(Listener, Callback);

        // 리스너 구독 추적
        if (!ListenerSubscriptions.Contains(Listener))
        {
            ListenerSubscriptions.Add(Listener, TArray<FPGEventKey>());
        }
        ListenerSubscriptions[Listener].AddUnique(Key);

        UE_LOG(LogTemp, Log, TEXT("Subscribed %s to event: %s"), 
               *Listener->GetClass()->GetName(), *Key.ToString());
    }

    // 람다 구독
    template<typename TEnum, typename TCallable>
    void SubscribeLambda(TEnum EventType, TCallable&& Callback)
    {
        FPGEventKey Key = MakeEventKey(EventType);

        if (!EventDelegates.Contains(Key))
        {
            EventDelegates.Add(Key, FUniversalEventDelegate());
        }

        EventDelegates[Key].AddLambda([Callback](FPGEventKey EventKey, UObject* EventData)
        {
            // FEventKey를 다시 원래 enum으로 변환해서 콜백 호출
            TEnum OriginalEnum = static_cast<TEnum>(EventKey.EventValue);
            Callback(OriginalEnum, EventData);
        });

        UE_LOG(LogTemp, Log, TEXT("Subscribed lambda to event: %s"), *Key.ToString());
    }

    // 이벤트 발송
    template<typename TEnum>
    void SendMessage(TEnum EventType, UObject* EventData = nullptr)
    {
        FPGEventKey Key = MakeEventKey(EventType);

        if (!EventDelegates.Contains(Key))
        {
            UE_LOG(LogTemp, Verbose, TEXT("No listeners for event: %s"), *Key.ToString());
            return;
        }

        UE_LOG(LogTemp, Log, TEXT("Sending event: %s"), *Key.ToString());
        
        if (EventData)
        {
            if (IPGEventData* EventInterface = Cast<IPGEventData>(EventData))
            {
                UE_LOG(LogTemp, Log, TEXT("Event Data: %s"), *EventInterface->ToString());
            }
        }

        EventDelegates[Key].Broadcast(Key, EventData);
    }

    // 조건부 발송
    template<typename TEnum, typename TPredicate>
    void SendMessageIf(TEnum EventType, UObject* EventData, TPredicate Predicate)
    {
        if (Predicate(EventType, EventData))
        {
            SendMessage(EventType, EventData);
        }
    }

    // 구독 해제
    template<typename TEnum>
    void Unsubscribe(TEnum EventType, UObject* Listener)
    {
        FPGEventKey Key = MakeEventKey(EventType);

        if (!Listener || !EventDelegates.Contains(Key))
        {
            return;
        }

        EventDelegates[Key].RemoveAll(Listener);

        // 리스너 구독 목록에서 제거
        if (ListenerSubscriptions.Contains(Listener))
        {
            ListenerSubscriptions[Listener].Remove(Key);
            if (ListenerSubscriptions[Listener].Num() == 0)
            {
                ListenerSubscriptions.Remove(Listener);
            }
        }

        UE_LOG(LogTemp, Log, TEXT("Unsubscribed %s from event: %s"), 
               *Listener->GetClass()->GetName(), *Key.ToString());
    }

    // 모든 이벤트에서 구독 해제
    void UnsubscribeAll(UObject* Listener)
    {
        if (!Listener || !ListenerSubscriptions.Contains(Listener))
        {
            return;
        }

        TArray<FPGEventKey> SubscribedEvents = ListenerSubscriptions[Listener];
        for (const FPGEventKey& Key : SubscribedEvents)
        {
            if (EventDelegates.Contains(Key))
            {
                EventDelegates[Key].RemoveAll(Listener);
            }
        }

        ListenerSubscriptions.Remove(Listener);
        UE_LOG(LogTemp, Log, TEXT("Unsubscribed %s from all events"), *Listener->GetClass()->GetName());
    }

    // 유틸리티 함수들
    template<typename TEnum>
    bool HasListeners(TEnum EventType) const
    {
        FPGEventKey Key = MakeEventKey(EventType);
        return EventDelegates.Contains(Key) && EventDelegates[Key].IsBound();
    }

private:
    // enum을 FEventKey로 변환하는 헬퍼 함수들
    template<typename TEnum>
    FPGEventKey MakeEventKey(TEnum EventType) const
    {
        FString TypeName = TEnum::StaticEnum()->GetName();
        int32 EventValue = static_cast<int32>(EventType);
        return FPGEventKey(TypeName, EventValue);
    }
};