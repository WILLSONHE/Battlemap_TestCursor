#include "MoveCommandMarkerActor.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

AMoveCommandMarkerActor::AMoveCommandMarkerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	SetRootComponent(MarkerMesh);
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerMesh->SetGenerateOverlapEvents(false);
	MarkerMesh->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MarkerMeshAsset(TEXT("/Game/Cursor/SM_CursorMesh.SM_CursorMesh"));
	if (MarkerMeshAsset.Succeeded())
	{
		MarkerMesh->SetStaticMesh(MarkerMeshAsset.Object);
		MarkerMesh->SetRelativeScale3D(FVector(3.0f, 3.0f, 3.0f));
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackMeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (FallbackMeshAsset.Succeeded())
		{
			MarkerMesh->SetStaticMesh(FallbackMeshAsset.Object);
			MarkerMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.15f));
		}
	}
}

void AMoveCommandMarkerActor::SetMarkerVisible(bool bVisible)
{
	if (!MarkerMesh)
	{
		return;
	}

	MarkerMesh->SetVisibility(bVisible, true);
	SetActorHiddenInGame(!bVisible);
}
