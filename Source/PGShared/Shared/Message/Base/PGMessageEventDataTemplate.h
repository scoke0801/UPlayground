// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "PGMessageEventDataBase.h"

template<typename T>
class FPGEventDataOneParam : public IPGEventData
{
public:
    T Value;
    
    FPGEventDataOneParam(T NewValue) : Value(NewValue) {}

    virtual ~FPGEventDataOneParam() override = default;
    virtual FString ToString() const { return TEXT("FPGEventDataOneParam"); }
};

template<typename ParamA, typename ParamB>
class FPGEventDataTwoParam : public IPGEventData
{
public:
    ParamA ValueA;
    ParamB ValueB;
    
    FPGEventDataTwoParam(ParamA InValueA, ParamB InValueB) : ValueA(InValueA), ValueB(InValueB) {}
    virtual ~FPGEventDataTwoParam() override  = default;
    virtual FString ToString() const { return TEXT("FPGEventDataTwoParam"); }
};
