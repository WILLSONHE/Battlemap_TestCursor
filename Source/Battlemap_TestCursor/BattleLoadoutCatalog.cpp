#include "BattleLoadoutCatalog.h"

namespace
{
	struct FTrip
	{
		const TCHAR* B;
		const TCHAR* C;
		const TCHAR* U;
	};

	// PROJECT_SUMMARY - Modified.md §7 (updated)
	const FTrip GRows[] = {
		{TEXT("\u9646\u519b"), TEXT("\u6b65\u5175"), TEXT("\u57fa\u7840\u6b65\u5175\u73ed")},
		{TEXT("\u9646\u519b"), TEXT("\u6b65\u5175"), TEXT("\u673a\u68b0\u5316\u6b65\u5175\u73ed")},
		{TEXT("\u9646\u519b"), TEXT("\u6b65\u5175"), TEXT("\u9a91\u5175")},
		{TEXT("\u9646\u519b"), TEXT("\u6b65\u6218\u8f66"), TEXT("8x8\u8f6e\u5f0f\u6b65\u6218\u8f66")},
		{TEXT("\u9646\u519b"), TEXT("\u6b65\u6218\u8f66"), TEXT("03\u5f0f\u7a7a\u964d\u8f7b\u578b\u5c65\u5e26\u6b65\u6218\u8f66")},
		{TEXT("\u9646\u519b"), TEXT("\u6b65\u6218\u8f66"), TEXT("04A\u5f0f\u5c65\u5e26\u6b65\u6218\u8f66")},
		{TEXT("\u9646\u519b"), TEXT("\u6b65\u6218\u8f66"), TEXT("05A\u5f0f\u4e24\u6816\u88c5\u7532\u7a81\u51fb\u8f66")},
		{TEXT("\u9646\u519b"), TEXT("\u5766\u514b"), TEXT("99A\u5f0f\u5766\u514b")},
		{TEXT("\u9646\u519b"), TEXT("\u5766\u514b"), TEXT("15\u5f0f\u8f7b\u578b\u5766\u514b")},
		{TEXT("\u9646\u519b"), TEXT("\u52a0\u69b4\u70ae"), TEXT("155\u6beb\u7c73\u8f66\u8f7d\u52a0\u69b4\u70ae")},
		{TEXT("\u9646\u519b"), TEXT("\u52a0\u69b4\u70ae"), TEXT("05A\u5f0f\u5c65\u5e23155\u6beb\u7c73\u52a0\u69b4\u70ae")},
		{TEXT("\u9646\u519b"), TEXT("\u5bfc\u5f39"), TEXT("\u7ea2\u7bad-10\u591a\u7528\u9014\u5bfc\u5f39")},
		{TEXT("\u9646\u519b"), TEXT("\u5bfc\u5f39"), TEXT("\u7ea2\u65d7-17A\u5730\u7a7a\u5bfc\u5f39")},
		{TEXT("\u6d77\u519b"), TEXT("\u6b65\u5175"), TEXT("\u57fa\u7840\u6b65\u5175\u73ed")},
		{TEXT("\u6d77\u519b"), TEXT("\u6b65\u5175"), TEXT("\u673a\u68b0\u5316\u6b65\u5175\u73ed")},
		{TEXT("\u6d77\u519b"), TEXT("\u6f5c\u8247"), TEXT("093A\u6838\u6f5c\u8247")},
		{TEXT("\u6d77\u519b"), TEXT("\u6f5c\u8247"), TEXT("039A\u6f5c\u8247")},
		{TEXT("\u6d77\u519b"), TEXT("\u6f5c\u8247"), TEXT("039B\u6f5c\u8247")},
		{TEXT("\u6d77\u519b"), TEXT("\u62a4\u536b\u8230"), TEXT("054A\u62a4\u536b\u8230")},
		{TEXT("\u6d77\u519b"), TEXT("\u62a4\u536b\u8230"), TEXT("056A\u62a4\u536b\u8230")},
		{TEXT("\u6d77\u519b"), TEXT("\u9a71\u9010\u8230"), TEXT("052C\u9a71\u9010\u8230")},
		{TEXT("\u6d77\u519b"), TEXT("\u9a71\u9010\u8230"), TEXT("052D\u9a71\u9010\u8230")},
		{TEXT("\u6d77\u519b"), TEXT("\u9a71\u9010\u8230"), TEXT("055\u9a71\u9010\u8230")},
		{TEXT("\u6d77\u519b"), TEXT("\u822a\u6bcd"), TEXT("\u5c71\u4e1c\u8230")},
		{TEXT("\u6d77\u519b"), TEXT("\u822a\u6bcd"), TEXT("\u8fbd\u5b81\u8230")},
		{TEXT("\u6d77\u519b"), TEXT("\u8865\u7ed9\u8230"), TEXT("901\u7efc\u5408\u8865\u7ed9\u8230")},
		{TEXT("\u6d77\u519b"), TEXT("\u8865\u7ed9\u8230"), TEXT("903\u7efc\u5408\u8865\u7ed9\u8230")},
		{TEXT("\u6d77\u519b"), TEXT("\u767b\u9646\u8247/\u4e24\u6816\u767b\u9646\u8230"), TEXT("726A\u6c14\u57ab\u767b\u9646\u8247")},
		{TEXT("\u6d77\u519b"), TEXT("\u767b\u9646\u8247/\u4e24\u6816\u767b\u9646\u8230"), TEXT("075\u4e24\u6816\u653b\u51fb\u8230")},
		{TEXT("\u6d77\u519b"), TEXT("\u767b\u9646\u8247/\u4e24\u6816\u767b\u9646\u8230"), TEXT("071\u7efc\u5408\u767b\u9646\u8230")},
		{TEXT("\u6d77\u519b"), TEXT("\u767b\u9646\u8247/\u4e24\u6816\u767b\u9646\u8230"), TEXT("072A\u767b\u9646\u8230")},
		{TEXT("\u6d77\u519b"), TEXT("\u63f4\u6f5c\u6551\u751f\u8230"), TEXT("926\u63f4\u6f5c\u6551\u751f\u8230")},
		{TEXT("\u7a7a\u519b"), TEXT("\u6b65\u5175"), TEXT("\u57fa\u7840\u6b65\u5175\u73ed")},
		{TEXT("\u7a7a\u519b"), TEXT("\u6b65\u5175"), TEXT("\u673a\u68b0\u5316\u6b65\u5175\u73ed")},
		{TEXT("\u7a7a\u519b"), TEXT("\u76f4\u5347\u673a"), TEXT("\u76f4-10\u76f4\u5347\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u76f4\u5347\u673a"), TEXT("\u76f4-19\u76f4\u5347\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u76f4\u5347\u673a"), TEXT("\u76f4-20\u6218\u672f\u901a\u7528\u76f4\u5347\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u76f4\u5347\u673a"), TEXT("\u76f4-8L\u76f4\u5347\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u6218\u6597\u673a"), TEXT("\u6b7c-10\u98de\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u6218\u6597\u673a"), TEXT("\u6b7c-15\u98de\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u6218\u6597\u673a"), TEXT("\u6b7c-16\u98de\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u6218\u6597\u673a"), TEXT("\u6b7c-20\u98de\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u8f70\u70b8\u673a"), TEXT("\u8f70-6K\u98de\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u8fd0\u8f93\u673a/\u52a0\u6cb9\u673a"), TEXT("\u8fd0-20\u98de\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u8fd0\u8f93\u673a/\u52a0\u6cb9\u673a"), TEXT("\u8fd0-8\u6280\u672f\u4fa6\u5bdf\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u8fd0\u8f93\u673a/\u52a0\u6cb9\u673a"), TEXT("\u8f70\u6cb9-6\u98de\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u9884\u8b66\u673a"), TEXT("\u7a7a\u8b66-500\u9884\u8b66\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u9884\u8b66\u673a"), TEXT("\u7a7a\u8b66-2000\u9884\u8b66\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u65e0\u4eba\u673a"), TEXT("\u65e0\u4fa6-7\u65e0\u4eba\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u65e0\u4eba\u673a"), TEXT("\u653b\u51fb-2\u65e0\u4eba\u673a")},
		{TEXT("\u7a7a\u519b"), TEXT("\u65e0\u4eba\u673a"), TEXT("\u653b\u51fb-11\u65e0\u4eba\u673a")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u6b65\u5175"), TEXT("\u57fa\u7840\u6b65\u5175\u73ed")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u6b65\u5175"), TEXT("\u673a\u68b0\u5316\u6b65\u5175\u73ed")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u706b\u7bad\u70ae"), TEXT("\u8fdc\u7a0b\u7bb1\u5f0f\u706b\u7bad\u70ae")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u4e1c\u98ce"), TEXT("\u4e1c\u98ce-16\u5bfc\u5f39")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u4e1c\u98ce"), TEXT("\u4e1c\u98ce-17\u5bfc\u5f39")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u4e1c\u98ce"), TEXT("\u4e1c\u98ce-21D\u5bfc\u5f39")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u4e1c\u98ce"), TEXT("\u4e1c\u98ce-26\u5bfc\u5f39")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u4e1c\u98ce"), TEXT("\u4e1c\u98ce-31AG\u5bfc\u5f39")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u4e1c\u98ce"), TEXT("\u4e1c\u98ce-41\u5bfc\u5f39")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u7ea2\u65d7"), TEXT("\u7ea2\u65d7-9B\u5730\u7a7a\u5bfc\u5f39")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u7ea2\u65d7"), TEXT("\u7ea2\u65d7-22\u5730\u7a7a\u5bfc\u5f39")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u957f\u5251"), TEXT("\u957f\u5251-10A\u5bfc\u5f39")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u957f\u5251"), TEXT("\u957f\u5251-100\u5bfc\u5f39")},
		{TEXT("\u706b\u7bad\u519b"), TEXT("\u9e70\u51fb"), TEXT("\u9e70\u51fb-62A\u5cb8\u8230\u5bfc\u5f39")},
		{TEXT("\u4fe1\u606f\u652f\u63f4"), TEXT("\u6b65\u5175"), TEXT("\u57fa\u7840\u6b65\u5175\u73ed")},
		{TEXT("\u4fe1\u606f\u652f\u63f4"), TEXT("\u6b65\u5175"), TEXT("\u673a\u68b0\u5316\u6b65\u5175\u73ed")},
		{TEXT("\u4fe1\u606f\u652f\u63f4"), TEXT("\u7535\u5b50\u5bf9\u6297"), TEXT("\u7535\u5b50\u6218\u961f\u4f0d")},
		{TEXT("\u4fe1\u606f\u652f\u63f4"), TEXT("\u96f7\u8fbe\u536b\u661f"), TEXT("\u96f7\u8fbe\u4fa6\u5bdf\u536b\u661f")},
		{TEXT("\u8054\u52e4\u4fdd\u969c"), TEXT("\u6b65\u5175"), TEXT("\u57fa\u7840\u6b65\u5175\u73ed")},
		{TEXT("\u8054\u52e4\u4fdd\u969c"), TEXT("\u6b65\u5175"), TEXT("\u673a\u68b0\u5316\u6b65\u5175\u73ed")},
		{TEXT("\u8054\u52e4\u4fdd\u969c"), TEXT("\u91ce\u6218\u533b\u9662"), TEXT("\u91ce\u6218\u533b\u7597\u961f")},
		{TEXT("\u8054\u52e4\u4fdd\u969c"), TEXT("\u5de5\u7a0b\u90e8\u961f"), TEXT("\u5de5\u5175\u8fde")},
	};
}

TArray<FString> BattleLoadoutCatalog::GetBranchOptions()
{
	TArray<FString> Out;
	for (const FTrip& T : GRows)
	{
		const FString B(T.B);
		if (!Out.Contains(B))
		{
			Out.Add(B);
		}
	}
	return Out;
}

TArray<FString> BattleLoadoutCatalog::GetClassOptions(const FString& Branch)
{
	TArray<FString> Out;
	for (const FTrip& T : GRows)
	{
		if (Branch == T.B)
		{
			const FString C(T.C);
			if (!Out.Contains(C))
			{
				Out.Add(C);
			}
		}
	}
	return Out;
}

TArray<FString> BattleLoadoutCatalog::GetUnitOptions(const FString& Branch, const FString& ClassName)
{
	TArray<FString> Out;
	for (const FTrip& T : GRows)
	{
		if (Branch == T.B && ClassName == T.C)
		{
			Out.Add(T.U);
		}
	}
	return Out;
}
