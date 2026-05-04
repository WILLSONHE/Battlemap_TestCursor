#pragma once

#include "CoreMinimal.h"

/** Static 兵种|分类|单位 tree from design doc (PROJECT_SUMMARY §7). */
namespace BattleLoadoutCatalog
{
	TArray<FString> GetBranchOptions();
	TArray<FString> GetClassOptions(const FString& Branch);
	TArray<FString> GetUnitOptions(const FString& Branch, const FString& ClassName);
}
