#include "PGNiagaraFXTools.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraRendererProperties.h"
#include "Serialization/JsonSerializer.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

FString UPGNiagaraFXTools::DescribeSystem(UNiagaraSystem* System)
{
    if (!System) return TEXT("{}");
    System->WaitForCompilationComplete(false, false);
    const auto DescribeStore = [](const FNiagaraParameterStore& Store)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        TArray<FNiagaraVariable> Variables;
        Store.GetParameters(Variables);
        for (auto& Variable : Variables)
        {
            auto Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("name"), Variable.GetName().ToString());
            Item->SetStringField(TEXT("type"), Variable.GetType().GetName());
            if (const uint8* Data = Store.GetParameterData(Variable))
                Item->SetStringField(TEXT("value"), Variable.GetType().ToString(Data));
            Result.Add(MakeShared<FJsonValueObject>(Item));
        }
        return Result;
    };
    auto Root = MakeShared<FJsonObject>();
    Root->SetBoolField(TEXT("compiled"), System->IsValid() && !System->HasOutstandingCompilationRequests());
    Root->SetArrayField(TEXT("user"), DescribeStore(System->GetExposedParameters()));
    TArray<TSharedPtr<FJsonValue>> Emitters;
    for (const auto& Handle : System->GetEmitterHandles())
    {
        auto Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("name"), Handle.GetName().ToString());
        Item->SetBoolField(TEXT("enabled"), Handle.GetIsEnabled());
        if (auto* Data = Handle.GetEmitterData())
        {
            Item->SetBoolField(TEXT("local_space"), Data->bLocalSpace);
            TArray<UNiagaraScript*> Scripts;
            Data->GetScripts(Scripts, false);
            TArray<TSharedPtr<FJsonValue>> ScriptInfo;
            for (auto* Script : Scripts)
            {
                auto S = MakeShared<FJsonObject>();
                S->SetStringField(TEXT("name"), Script->GetName());
                S->SetArrayField(TEXT("inputs"), DescribeStore(Script->RapidIterationParameters));
                ScriptInfo.Add(MakeShared<FJsonValueObject>(S));
            }
            Item->SetArrayField(TEXT("scripts"), ScriptInfo);
            TArray<TSharedPtr<FJsonValue>> Renderers;
            for (auto* Renderer : Data->GetRenderers())
            {
                FString Properties;
                for (TFieldIterator<FProperty> It(Renderer->GetClass()); It; ++It)
                {
                    if (!It->GetName().Contains(TEXT("Material")) && !It->GetName().Contains(TEXT("Mesh"))) continue;
                    FString Value;
                    It->ExportText_InContainer(0, Value, Renderer, nullptr, Renderer, PPF_None);
                    Properties += It->GetName() + TEXT("=") + Value + TEXT("\n");
                }
                Renderers.Add(MakeShared<FJsonValueString>(Renderer->GetClass()->GetName() + TEXT("\n") + Properties));
            }
            Item->SetArrayField(TEXT("renderers"), Renderers);
        }
        Emitters.Add(MakeShared<FJsonValueObject>(Item));
    }
    Root->SetArrayField(TEXT("emitters"), Emitters);
    FString Result;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Result));
    return Result;
}

bool UPGNiagaraFXTools::ConfigureSlash(UNiagaraSystem* System)
{
    if (!System || !System->GetPathName().StartsWith(TEXT("/Game/Art/PlayerCombatFX/"))) return false;
    System->Modify();
    const FNiagaraVariable Tint(FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.SlashTint"));
    const FNiagaraVariable Alpha(FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.SlashAlpha"));
    System->GetExposedParameters().SetParameterValue(FVector3f(1.f), Tint, true);
    System->GetExposedParameters().SetParameterValue(1.f, Alpha, true);
    int32 Bindings = 0;
    for (auto& Handle : System->GetEmitterHandles())
    {
        if (Handle.GetName() == TEXT("Feather")) { Handle.SetIsEnabled(false, *System, false); continue; }
        auto* Data = Handle.GetEmitterData();
        if (!Data) continue;
        Data->bLocalSpace = true;
        Data->SimTarget = ENiagaraSimTarget::CPUSim;
        Data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Dynamic;
        auto* Source = Cast<UNiagaraScriptSource>(Data->GraphSource);
        if (!Source || !Source->NodeGraph)
        {
            UE_LOG(LogTemp, Error, TEXT("PGPlayerNiagara missing graph: %s"), *Handle.GetName().ToString());
            return false;
        }
        TArray<UNiagaraNodeFunctionCall*> Functions;
        Source->NodeGraph->GetNodesOfClass(Functions);
        for (auto* Function : Functions)
        {
            if (!Function->FunctionScript || Function->FunctionScript->GetName() != TEXT("Color")) continue;
            for (const auto& Variable : {Tint, Alpha})
            {
                const auto Input = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
                    FNiagaraParameterHandle(Variable == Tint ? TEXT("Module.Scale Color") : TEXT("Module.Scale Alpha")), Function);
                auto& Pin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
                    *Function, Input, Variable.GetType(), FGuid(), FGuid());
                if (!Pin.LinkedTo.IsEmpty())
                {
                    if (Pin.LinkedTo.Num() != 1 || Pin.LinkedTo[0]->PinName != Variable.GetName())
                    {
                        UE_LOG(LogTemp, Error, TEXT("PGPlayerNiagara unexpected binding: %s %s"), *Handle.GetName().ToString(), *Function->GetFunctionName());
                        return false;
                    }
                }
                else FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin, Variable, {Tint, Alpha});
            }
            ++Bindings;
        }
        Source->NodeGraph->NotifyGraphChanged();
    }
    System->RequestCompile(true);
    System->WaitForCompilationComplete(false, false);
    System->MarkPackageDirty();
    // IsReadyToRun deliberately returns false in NullRHI commandlets.
    const bool bCompiled = System->IsValid() && !System->HasOutstandingCompilationRequests();
    UE_LOG(LogTemp, Display, TEXT("PGPlayerNiagara configure bindings=%d compiled=%d"), Bindings, bCompiled);
    return Bindings >= 3 && bCompiled;
}
