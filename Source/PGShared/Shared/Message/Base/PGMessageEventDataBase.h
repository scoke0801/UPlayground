// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// 이벤트 데이터 인터페이스
class IPGEventData
{
public:
    virtual ~IPGEventData() = default;
    virtual FString ToString() const { return FString();};
};

// 메시지 델리게이트 선언
DECLARE_MULTICAST_DELEGATE_OneParam(FPGMessageDelegate, const IPGEventData*);
