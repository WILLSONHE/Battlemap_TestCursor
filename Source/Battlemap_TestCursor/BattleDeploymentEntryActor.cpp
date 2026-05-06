#include "BattleDeploymentEntryActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABattleDeploymentEntryActor::ABattleDeploymentEntryActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(true);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(Root);

	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh"));
	PlaneMesh->SetupAttachment(Root);
	PlaneMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PlaneMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PlaneMesh->SetGenerateOverlapEvents(false);
	PlaneMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneAsset.Succeeded())
	{
		PlaneMesh->SetStaticMesh(PlaneAsset.Object);
	}
	// Keep plane parallel to the map (XY); old -90 pitch made it edge-on from typical top-down views.
	PlaneMesh->SetRelativeRotation(FRotator::ZeroRotator);
	PlaneMesh->SetRelativeLocation(FVector(0.f, 0.f, 12.f));
	PlaneMesh->SetRelativeScale3D(FVector(6.f, 3.f, 1.f));

	LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LabelText"));
	LabelText->SetupAttachment(Root);
	LabelText->SetRelativeLocation(FVector(0.f, 0.f, 35.f));
	LabelText->SetHorizontalAlignment(EHTA_Center);
	LabelText->SetVerticalAlignment(EVRTA_TextCenter);
	LabelText->SetWorldSize(220.f);
	LabelText->SetText(FText::FromString(TEXT("\u8fdb\u5165\u6218\u573a")));
	LabelText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABattleDeploymentEntryActor::ApplyBattlePresentation(float TileWorldUU)
{
	const float WUU = FMath::Clamp(TileWorldUU * 5.f, 600.f, 4000.f);
	const float DUU = WUU * 0.48f;
	// Engine plane mesh is 100x100 uu before scale.
	PlaneMesh->SetRelativeScale3D(FVector(WUU / 100.f, DUU / 100.f, 1.f));
	PlaneMesh->SetRelativeLocation(FVector(0.f, 0.f, FMath::Max(25.f, TileWorldUU * 0.08f)));

	const float FontWU = FMath::Clamp(TileWorldUU * 0.42f, 220.f, 640.f);
	LabelText->SetWorldSize(FontWU);
	LabelText->SetRelativeLocation(FVector(0.f, 0.f, FMath::Max(45.f, TileWorldUU * 0.15f)));
	LabelText->SetTextRenderColor(bPlayerOwned ? FColor(40, 255, 120) : FColor(255, 70, 70));

	if (UMaterialInstanceDynamic* MID = PlaneMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		const FLinearColor Tint = bPlayerOwned ? FLinearColor(0.08f, 0.55f, 0.18f, 1.f) : FLinearColor(0.55f, 0.1f, 0.1f, 1.f);
		MID->SetVectorParameterValue(FName(TEXT("PaintColor")), Tint);
		MID->SetVectorParameterValue(FName(TEXT("Tint")), Tint);
		MID->SetVectorParameterValue(FName(TEXT("BaseColor")), Tint);
		MID->SetVectorParameterValue(FName(TEXT("Color")), Tint);
	}
}
