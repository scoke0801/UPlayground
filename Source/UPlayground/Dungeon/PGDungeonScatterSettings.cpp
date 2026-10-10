#include "PGDungeonScatterSettings.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Engine/StaticMesh.h"
#include "PGData/DataAsset/Dungeon/PGDungeonDefinition.h"
#include "PGData/Dungeon/PGDungeonLayoutBuilder.h"

class FPGDungeonScatterElement : public IPCGElement
{
public:
    virtual bool IsCacheable(const UPCGSettings*) const override { return false; }
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext*) const override { return true; }
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override
    {
        const auto* Settings = Context->GetInputSettings<UPGDungeonScatterSettings>();
        const auto* D = Settings->Definition.Get();
        if (!D || !FMath::IsFinite(Settings->Density)) return true;
        const auto& Layout = Settings->Layout;
        const float Half = D->RoomSize*.5f, Spacing = D->RoomSize+D->CorridorLength;
        auto Center = [&](int32 Id) { const auto P=Layout.Rooms[Id].Cell; return Settings->Origin+FVector(P.X,P.Y,0)*Spacing; };
        TArray<FBox> Exclusions;
        for (const auto& Room : Layout.Rooms)
            Exclusions.Emplace(Center(Room.Id)-FVector(Half+500,Half+500,2000),Center(Room.Id)+FVector(Half+500,Half+500,2000));
        for (const auto& Link : Layout.Connections)
        {
            const FVector C=(Center(Link.A)+Center(Link.B))*.5f;
            const bool X=Layout.Rooms[Link.A].Cell.X!=Layout.Rooms[Link.B].Cell.X;
            const FVector E=X ? FVector(D->CorridorLength*.5f+500,D->CorridorWidth*.5f+500,2000) : FVector(D->CorridorWidth*.5f+500,D->CorridorLength*.5f+500,2000);
            Exclusions.Emplace(C-E,C+E);
        }
        const FIntPoint Directions[]={{1,0},{0,1},{-1,0},{0,-1}};
        for (int32 Kind=0; Kind<2; ++Kind)
        {
            auto Meshes=Kind ? D->ExteriorMeshes : D->RuinMeshes;
            Meshes.Sort([](const auto& A,const auto& B){ return A.ToSoftObjectPath().ToString()<B.ToSoftObjectPath().ToString(); });
            TArray<UStaticMesh*> Loaded;
            TArray<UPCGPointData*> Groups;
            for (const auto& Mesh : Meshes)
            {
                auto* Asset=Mesh.LoadSynchronous(); if (!Asset) continue;
                Loaded.Add(Asset);
                auto* Points=NewObject<UPCGPointData>(); Groups.Add(Points);
                auto& Output=Context->OutputData.TaggedData.Emplace_GetRef();
                Output.Data=Points; Output.Pin=PCGPinConstants::DefaultOutputLabel;
                Output.Tags.Add(Asset->GetPathName());
            }
            if (Loaded.IsEmpty()) continue;
            for (const auto& Room : Layout.Rooms)
            {
                FRandomStream Random(PGDungeon::StreamSeed(Layout.Seed,Kind ? 4 : 3,Room.Id));
                const int32 Count=FMath::Clamp(FMath::RoundToInt((Kind ? D->ExteriorAttemptsPerRoom : D->PropsPerRoom)*Settings->Density),0,Kind ? 64 : 8);
                for (int32 I=0; I<Count; ++I)
                {
                    const int32 Group=Random.RandRange(0,Loaded.Num()-1);
                    const FBox Bounds=Loaded[Group]->GetBoundingBox();
                    const float Target=Kind ? Random.FRandRange(650,950) : Random.FRandRange(90,160);
                    const double Scale=Target/FMath::Max(1.,Kind ? Bounds.GetSize().Z : Bounds.GetSize().GetMax());
                    const FQuat Rotation(FVector::UpVector,Random.FRand()*2*PI);
                    FVector Position;
                    if (Kind)
                    {
                        const auto Dir=Directions[I%4];
                        Position=Center(Room.Id)+FVector(Dir.X,Dir.Y,0)*(Half+Random.FRandRange(1100,2100))+
                            FVector(-Dir.Y,Dir.X,0)*Random.FRandRange(-Half,Half);
                        const double Radius=Bounds.GetExtent().Size2D()*Scale;
                        bool Excluded=false;
                        for (const FBox& Box:Exclusions) if (Box.ExpandBy(Radius).IsInsideOrOn(Position)) { Excluded=true; break; }
                        if (Excluded) continue;
                        Position.Z-=130;
                    }
                    else
                    {
                        // Tiny rubble only in corner strips outside the clear combat square and socket cross.
                        Position=Center(Room.Id)+FVector(I%2 ? 1:-1,(I/2)%2 ? 1:-1,0)*(Half-170);
                        if (I>=4) Position+=FVector(I%2 ? -170:170,(I/2)%2 ? -170:170,0);
                    }
                    Position-=Rotation.RotateVector(FVector(Bounds.GetCenter().X,Bounds.GetCenter().Y,Bounds.Min.Z)*Scale);
                    FPCGPoint Point; Point.Transform=FTransform(Rotation,Position,FVector(Scale));
                    Point.Seed=Random.GetCurrentSeed(); Point.BoundsMin=Bounds.Min; Point.BoundsMax=Bounds.Max;
                    Groups[Group]->GetMutablePoints().Add(Point);
                }
            }
        }
        return true;
    }
};

FPCGElementPtr UPGDungeonScatterSettings::CreateElement() const { return MakeShared<FPGDungeonScatterElement>(); }
