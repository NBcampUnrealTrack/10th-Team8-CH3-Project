#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MonsterProgressionLibrary.generated.h"

class AActor;

//몬스터 등급 스펙 배율과 경험치가 전부 이 값 하나로 갈림
//엘리트인지 보스인지를 쓰는 쪽마다 따로 판단하지 않게 하려고 값으로 뽑아 둠
//케디스님이 엘리트 클래스를 더 만들어도 GetMonsterGrade만 고치면 나머지는 그대로 돎
UENUM(BlueprintType)
enum class EMonsterGrade : uint8
{
	//일반 잡몹
	Normal,
	//엘리트 스폰 볼륨이 확률로 냄
	Elite,
	//보스 레벨마다 하나 Endless는 주기적으로
	Boss
};

//난이도가 몬스터 체력과 공격력에 거는 배율 순서는 쉬움 보통 어려움
//기획값 쉬움 1 보통 1.3 하드코어 1.5
//UDreamVeilGameInstance::GetSpawnCurveScale은 엘리트가 언제부터 많아지는지를 정하고 이쪽은 한 마리가 얼마나 센지를 정함
//둘을 나눠 둔 이유 한 값으로 묶으면 쉬움에서 엘리트를 늦게 내려다가 잡몹까지 같이 약해짐
const float MONSTER_STAT_SCALE_BY_DIFFICULTY[] = { 1.0f, 1.3f, 1.5f };

//레벨이 올라갈수록 몬스터가 세지는 배율 0번이 L1
//같은 레벨 안에서는 전부 같은 배율이라 L2의 잡몹은 어느 볼륨에서 나와도 스펙이 같음
const float MONSTER_STAT_SCALE_BY_STAGE[] = { 1.0f, 1.35f, 1.8f, 2.4f };

//등급이 거는 배율 엘리트는 잡몹 두 마리 몫 보스는 여섯 마리 몫
const float MONSTER_STAT_SCALE_BY_GRADE[] = { 1.0f, 2.2f, 6.0f };

//Endless에서 몬스터가 세지는 속도 1분마다 이 값만큼 배율이 더해짐
//레벨 배율과 달리 상한이 없음 오래 버틸수록 계속 세짐
const float ENDLESS_STAT_SCALE_PER_MINUTE = 0.25f;

//등급별 기본 경험치 L1에서 잡았을 때 받는 값
//플레이어는 레벨 1에서 2로 갈 때 100이 필요하므로 잡몹 17마리쯤이 한 레벨
//짜게 달라는 요청이라 한 웨이브(4마리)로는 레벨이 오르지 않게 잡음
const float MONSTER_EXPERIENCE_BY_GRADE[] = { 6.0f, 22.0f, 90.0f };

//등급별로 잡았을 때 내려가는 잠식도 0~1 단위
//레벨과 Endless가 같은 표를 씀 잠식도가 시간과 분리돼서 모드별 보정이 필요 없음
//채우는 시간 120초 기준 초당 0.83퍼센트씩 오르므로 잡몹을 1초에 한 마리씩 잡으면 거의 제자리
//그보다 빠르면 잠식도가 밀려나고 느려지면 밀린다 몬스터가 세질수록 자연히 밀리게 됨
const float MONSTER_CORRUPTION_RELIEF_BY_GRADE[] = { 0.01f, 0.04f, 0.10f };

//등급 수와 표 칸 수가 어긋나면 컴파일에서 바로 걸리게 막음
static_assert(static_cast<int32>(UE_ARRAY_COUNT(MONSTER_CORRUPTION_RELIEF_BY_GRADE)) == static_cast<int32>(EMonsterGrade::Boss) + 1, "MONSTER_CORRUPTION_RELIEF_BY_GRADE needs one value per grade");
static_assert(static_cast<int32>(UE_ARRAY_COUNT(MONSTER_STAT_SCALE_BY_GRADE)) == static_cast<int32>(EMonsterGrade::Boss) + 1, "MONSTER_STAT_SCALE_BY_GRADE needs one value per grade");
static_assert(static_cast<int32>(UE_ARRAY_COUNT(MONSTER_EXPERIENCE_BY_GRADE)) == static_cast<int32>(EMonsterGrade::Boss) + 1, "MONSTER_EXPERIENCE_BY_GRADE needs one value per grade");

//레벨과 난이도에 따라 몬스터가 얼마나 센지와 잡았을 때 줄 경험치를 한 곳에 모음
//꿈의 조각과 파츠 드롭은 이미 UInventoryComponent::ReceiveKillRewards가 하고 있어서 여기서 다시 하지 않음
//MonsterBase에 직접 넣지 않은 이유 스펙 계산은 레벨 진행 규칙이라 몬스터 자신이 알 일이 아니고
//잡몹 엘리트 보스가 각자 같은 계산을 베껴 쓰면 밸런스를 고칠 때 세 군데를 고쳐야 함
UCLASS()
class DREAMVEIL_API UMonsterProgressionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//이 액터가 어떤 등급의 몬스터인지 몬스터가 아니면 Normal
	//보스 판정은 AMainGameModeBase::IsBossMonster와 같은 기준을 써서 레벨 클리어 조건과 어긋나지 않게 함
	UFUNCTION(BlueprintPure, Category = "Monster|Progression")
	static EMonsterGrade GetMonsterGrade(const AActor* Monster);

	//지금 난이도가 거는 스펙 배율 게임 인스턴스가 없으면 1
	UFUNCTION(BlueprintPure, Category = "Monster|Progression", meta = (WorldContext = "WorldContextObject"))
	static float GetDifficultyStatScale(const UObject* WorldContextObject);

	//지금 맵이 거는 스펙 배율
	//L1~L4는 표에서 읽고 Endless는 맵을 연 뒤 흐른 시간으로 계산함
	//로비나 메인 메뉴면 1이라 테스트 맵에서 몬스터를 내도 스펙이 안 뒤틀림
	UFUNCTION(BlueprintPure, Category = "Monster|Progression", meta = (WorldContext = "WorldContextObject"))
	static float GetStageStatScale(const UObject* WorldContextObject);

	//등급이 거는 스펙 배율
	UFUNCTION(BlueprintPure, Category = "Monster|Progression")
	static float GetGradeStatScale(EMonsterGrade Grade);

	//최종 배율 난이도 x 레벨 x 등급
	//몬스터가 스탯을 초기화할 때 체력과 공격력에 이 값을 곱함
	UFUNCTION(BlueprintPure, Category = "Monster|Progression", meta = (WorldContext = "WorldContextObject"))
	static float GetMonsterStatScale(const UObject* WorldContextObject, EMonsterGrade Grade);

	//이 등급을 잡았을 때 줄 경험치
	//레벨 배율을 그대로 곱하면 뒤 레벨에서 경험치가 폭발해서 제곱근만 반영함
	//Endless도 같은 방식이라 오래 버텨도 경험치가 시간에 비례해서 늘지는 않음
	UFUNCTION(BlueprintPure, Category = "Monster|Progression", meta = (WorldContext = "WorldContextObject"))
	static float GetExperienceReward(const UObject* WorldContextObject, EMonsterGrade Grade);

	//잡은 쪽에게 경험치를 줌 죽은 대상이 몬스터가 아니거나 잡은 쪽이 플레이어가 아니면 아무 일도 없음
	//꿈의 조각 파츠와 같은 자리(AugmentDamageLibrary의 사망 처리)에서 불러서 판정이 두 벌로 갈라지지 않게 함
	UFUNCTION(BlueprintCallable, Category = "Monster|Progression")
	static void GrantKillExperience(AActor* Killer, AActor* DeadMonster);

	//잡은 만큼 잠식도를 내림 등급이 높을수록 많이 내려감
	//경험치와 같은 자리에서 불러서 "잡았다"는 판정이 두 벌로 갈라지지 않게 함
	//잡은 쪽을 따지지 않는 이유 화염이나 가시 반사로 죽어도 플레이어가 잡은 것이라 잠식도는 내려가야 함
	UFUNCTION(BlueprintCallable, Category = "Monster|Progression")
	static void GrantKillCorruptionRelief(AActor* DeadMonster);
};
