// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterSpawnVolume.h"
#include "DreamPlayPolish.h"
#include "DreamObjectPoolSubsystem.h"
#include "DreamCrowdTacticsSubsystem.h"
#include "MainGameModeBase.h"
#include "Kismet/GameplayStatics.h"

#include "MainPlayerCharacter.h"
#include "MonsterBase.h"
#include "EliteMonster.h"
#include "CombatStatsComponent.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DreamVeilGameInstance.h"
#include "DreamBalanceProfile.h"
#include "UObject/ConstructorHelpers.h"

//낼 자리를 몇 번까지 뽑아볼지
//튜닝할 값이 아니라 "한 번 실패했다고 포기하지 말자" 정도의 수라서 프로퍼티로 빼지 않음
const int32 SPAWN_LOCATION_TRY_COUNT = 5;

//경로로 몬스터 블루프린트를 찾아 목록에 넣음 못 찾으면 아무것도 넣지 않음
//생성자 안에서만 부를 것 ConstructorHelpers는 생성자 밖에서는 쓰지 못함
//같은 다섯 줄을 복사해 붙이지 않으려고 뺌
void AddMonsterClass(TArray<TSubclassOf<AMonsterBase>>& OutMonsters, const TCHAR* BlueprintPath)
{
	ConstructorHelpers::FClassFinder<AMonsterBase> MonsterFinder(BlueprintPath);

	if (MonsterFinder.Succeeded())
	{
		OutMonsters.Add(MonsterFinder.Class);
	}
}

AMonsterSpawnVolume::AMonsterSpawnVolume()
{
	MaxDifficultyLevel = 30;
	EliteMonsterMinRate = 5.0f;
	EliteMonsterMaxRate = 50.0f;
	DifficultyCurve = 1.0f;

	//레벨에 끌어다 놓기만 하면 바로 몬스터가 나오게 기본 목록을 코드가 채워둠
	//목록을 비워두면 볼륨을 놓아도 아무 일이 없는데 그게 배치한 사람 눈에는 고장난 것처럼 보임
	//디테일 패널에서 목록을 바꾸면 그쪽이 이김 볼륨마다 다른 몬스터를 내고 싶을 때 그렇게 쓸 것
	AddMonsterClass(BaseMonsters, TEXT("/Game/Blueprint/Monsters/BP_MeleeMonster"));
	AddMonsterClass(BaseMonsters, TEXT("/Game/Blueprint/Monsters/BP_RangedMonster"));

	//엘리트는 넣는 순서가 곧 해금 순서임 L1은 1번만 L2는 1 2번 L3부터 셋 다 나옴
	//BP_EliteMonster(번호 없는 것)를 넣지 않는 이유 쓰지 않는 블루프린트라 목록에 있으면 순서만 밀림
	AddMonsterClass(EliteMonsters, TEXT("/Game/Blueprint/Monsters/BP_EliteMonster1"));
	AddMonsterClass(EliteMonsters, TEXT("/Game/Blueprint/Monsters/BP_EliteMonster2"));
	AddMonsterClass(EliteMonsters, TEXT("/Game/Blueprint/Monsters/BP_EliteMonster3"));
}

void AMonsterSpawnVolume::ExecuteSpawnActor()
{
	bHasGroupOrigin=false;TrySpawnMonster();bHasGroupOrigin=false;
}

bool AMonsterSpawnVolume::TrySpawnMonster()
{
	FVector SpawnLocation;

	//땅 위 설 수 있는 자리를 못 찾으면 이번 한 마리는 건너뜀
	//공중이나 벽 속에 내면 몬스터가 끼거나 떨어져서 플레이어에게 오지 못함
	if (!FindSpawnLocation(SpawnLocation))
	{
		return false;
	}

	const float EliteSpawnRate = GetEliteRate();

	//이 레벨에서 나올 수 있는 엘리트만 추림 L1에서 3번 엘리트가 튀어나오지 않게
	const TArray<TSubclassOf<AMonsterBase>> AllowedElites = GetEliteMonstersForCurrentLevel();

	//엘리트에 당첨됐는데 낼 엘리트가 없으면 잡몹으로 내려감
	//여기서 그냥 돌아가면 엘리트를 안 채워둔 볼륨이 아무것도 안 내서 웨이브가 통째로 비어버림
    const auto* GI = GetGameInstance<UDreamVeilGameInstance>();
    const int32 Stage = GI ? GI->GetCurrentLevelNumber() : 1;
    const auto* Profile=GI?GI->GetBalanceProfile():nullptr;
    const int32 StageIndex=FMath::Clamp(Stage-1,0,3);
    const int32 EliteLimit=Profile?(GI->IsInEndless()?FMath::Max(0,Profile->EndlessEliteLimit):
        (Profile->StageEliteLimits.IsValidIndex(StageIndex)?FMath::Max(0,Profile->StageEliteLimits[StageIndex]):FMath::Clamp(Stage+2,3,6))):
        (GI && GI->IsInEndless()?8:FMath::Clamp(Stage+2,3,6));
    // Refresh once per pack, incrementing only after a successful elite acquisition.
    int32 LivingElites=CachedGroupEliteCount;
    if(LivingElites<0)
    {
        LivingElites=0;
        for(TActorIterator<AEliteMonster> It(GetWorld());It;++It)
            if(!It->IsHidden()&&It->MonsterCombatStats&&!It->MonsterCombatStats->IsDead())++LivingElites;
        if(bInsideSpawnGroup)CachedGroupEliteCount=LivingElites;
    }
    if (bPreferRangedMember)
    {
        TArray<TSubclassOf<AMonsterBase>> Ranged;
        for (auto Class : BaseMonsters) if (Class && Class->GetDefaultObject<AMonsterBase>()->GetAttackType()==EMonsterAttackType::Ranged) Ranged.Add(Class);
        if (Ranged.Num()) return SpawnOneMonster(Ranged,SpawnLocation)!=nullptr;
    }
    if (LivingElites < EliteLimit && FMath::FRandRange(0.0f, 100.0f) <= EliteSpawnRate && AllowedElites.Num() > 0)
	{
		auto* Elite=SpawnOneMonster(AllowedElites,SpawnLocation);
        if(Elite&&bInsideSpawnGroup)++CachedGroupEliteCount;
        return Elite!=nullptr;
	}

	return SpawnOneMonster(BaseMonsters, SpawnLocation) != nullptr;
}

//이 레벨에서 나올 수 있는 엘리트만 추려서 돌려줌
TArray<TSubclassOf<AMonsterBase>> AMonsterSpawnVolume::GetEliteMonstersForCurrentLevel() const
{
	const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	//Endless는 끝까지 간 뒤에 열리는 곳이라 전부 나옴
	if (!DreamVeilGameInstance || DreamVeilGameInstance->IsInEndless())
	{
		return EliteMonsters;
	}

	const int32 LevelNumber = DreamVeilGameInstance->GetCurrentLevelNumber();

	//레벨 맵이 아니면 0이 돌아옴 로비나 테스트 맵에서는 막을 이유가 없어서 전부 씀
	if (LevelNumber <= 0)
	{
		return EliteMonsters;
	}

	//레벨 번호가 곧 쓸 수 있는 개수 L4처럼 목록보다 레벨이 높으면 목록 전체가 됨
	const int32 AllowedCount = FMath::Min(LevelNumber, EliteMonsters.Num());

	return TArray<TSubclassOf<AMonsterBase>>(EliteMonsters.GetData(), AllowedCount);
}

bool AMonsterSpawnVolume::FindSafeSpawnPoint(FVector& Point)
{
    bHasGroupOrigin=false;const bool Found=FindSpawnLocation(Point);bHasGroupOrigin=false;return Found;
}
bool AMonsterSpawnVolume::FindSpawnLocation(FVector& OutLocation)
{
    auto* PawnCharacter=GetPlayerCharacter();const auto* GI=GetGameInstance<UDreamVeilGameInstance>();
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    const bool Endless=GI&&GI->IsInEndless();
    if(PawnCharacter&&GI&&(GI->IsInLevelMap()||Endless))
    {
        if(!Nav){++SpawnLocationFailures;return false;}
        const FVector PlayerFeet=PawnCharacter->GetNavAgentLocation();
        auto* NavData=Nav->GetNavDataForProps(PawnCharacter->GetNavAgentPropertiesRef(),PlayerFeet);
        FNavLocation PlayerPoint;
        if(!NavData){++SpawnLocationFailures;return false;}
        if(!Nav->ProjectPointToNavigation(PlayerFeet,PlayerPoint,FVector(300,300,350),NavData)
            &&!Nav->ProjectPointToNavigation(PlayerFeet,PlayerPoint,FVector(1200,1200,1200),NavData))
        {++SpawnLocationFailures;return false;}
        const float Inner=FMath::Max(MinPlayerDistance,Endless?1800.f:800.f);
        auto* PC=Cast<APlayerController>(PawnCharacter->GetController());FVector Eye;FRotator View;int32 Width=0,Height=0;
        if(PC){PC->GetPlayerViewPoint(Eye,View);PC->GetViewportSize(Width,Height);}
        int32 PathQueries=0;
        bool AllowVisibleFallback=false;
        float BodyRadius=50.f,BodyHalfHeight=95.f;
        TArray<TSubclassOf<AMonsterBase>> BodyClasses=BaseMonsters;BodyClasses.Append(GetEliteMonstersForCurrentLevel());
        for(const auto& Class:BodyClasses)if(Class)if(const auto* Capsule=Class->GetDefaultObject<AMonsterBase>()->GetCapsuleComponent())
        {BodyRadius=FMath::Max(BodyRadius,Capsule->GetScaledCapsuleRadius());BodyHalfHeight=FMath::Max(BodyHalfHeight,Capsule->GetScaledCapsuleHalfHeight());}
        auto SafePoint=[&](const FVector& Candidate,FVector& Result,const FVector& Extent)
        {
            FNavLocation Point;if(!Nav->ProjectPointToNavigation(Candidate,Point,Extent,NavData))return false;
            const double Distance=FVector::DistSquared2D(Point.Location,PlayerFeet);
            if(Distance<FMath::Square(Inner))return false;
            if(Endless&&PerimeterBounds.IsValid)
            {
                // Keep even fallback projections in the outer 40 m, inside the playable boundary.
                const auto& B=PerimeterBounds;
                const double Edge=FMath::Min(FMath::Min(Point.Location.X-B.Min.X,B.Max.X-Point.Location.X),FMath::Min(Point.Location.Y-B.Min.Y,B.Max.Y-Point.Location.Y));
                if(Edge<150||Edge>4000)return false;
            }
            FCollisionQueryParams Q(SCENE_QUERY_STAT(SafeMonsterSpawn),false,PawnCharacter);
            const FVector Body=Point.Location+FVector(0,0,BodyHalfHeight+10);
            if(GetWorld()->OverlapBlockingTestByChannel(Body,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(BodyRadius,BodyHalfHeight),Q))return false;
            // Distant edge packs may be seen approaching. Nearby spawns must remain occluded.
            if(PC&&Width>0&&Height>0&&Distance<FMath::Square(Endless?6000.f:3500.f))
            {
                FVector2D Screen;
                if(PC->ProjectWorldLocationToScreen(Body,Screen,true)&&Screen.X>=-80&&Screen.Y>=-80&&Screen.X<=Width+80&&Screen.Y<=Height+80)
                {
                    FHitResult Hit;if(!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Body,ECC_Visibility,Q))
                    {
                        // Prefer occluded points. A fully visible room must not disable spawning forever.
                        const bool Central=Screen.X>Width*.25f&&Screen.X<Width*.75f&&Screen.Y>Height*.2f&&Screen.Y<Height*.8f;
                        if(!AllowVisibleFallback||Distance<FMath::Square(FMath::Max(Inner,1200.f))||(Central&&Distance<FMath::Square(2200.f)))return false;
                    }
                }
            }
            if(++PathQueries>12)return false;
            FPathFindingQuery Path(PawnCharacter,*NavData,Point.Location,PlayerPoint.Location);Path.SetAllowPartialPaths(false);
            if(!Nav->TestPathSync(Path))return false;
            Result=Point.Location;return true;
        };
        if(bHasGroupOrigin)
        {
            for(int32 I=0;I<12;++I)
            {
                const float A=I*UE_TWO_PI/12+FMath::FRandRange(-.15f,.15f);
                if(SafePoint(GroupOrigin+FVector(FMath::Cos(A),FMath::Sin(A),0)*FMath::FRandRange(160.f,400.f),OutLocation,FVector(140,140,200))&&FVector::DistSquared2D(OutLocation,GroupOrigin)<=FMath::Square(500.f))return true;
            }
            // Preserve the pack origin, but probe additional neighbouring offsets if another pawn occupies it.
            for(int32 I=0;I<12;++I)
            {
                const float Angle=I*UE_TWO_PI/12;PathQueries=0;
                if(SafePoint(GroupOrigin+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*FMath::FRandRange(400.f,650.f),OutLocation,FVector(200,200,300)))return true;
            }
            ++SpawnLocationFailures;return false;
        }
        const int32 Sequence=SpawnSectorSequence++;
        if(Endless)
        {
            const float Now=GetWorld()->GetTimeSeconds();
            if(!PerimeterBounds.IsValid||Now>=NextBoundsRefresh)
            {
                PerimeterBounds.Init();NextBoundsRefresh=Now+15;
                for(TActorIterator<ANavMeshBoundsVolume> It(GetWorld());It;++It)
                {
                    const FBox B=It->GetComponentsBoundingBox(true);
                    if(B.IsInsideXY(PlayerFeet)&&(!PerimeterBounds.IsValid||B.GetExtent().X*B.GetExtent().Y>PerimeterBounds.GetExtent().X*PerimeterBounds.GetExtent().Y))PerimeterBounds=B;
                }
            }
            if(!PerimeterBounds.IsValid){++SpawnLocationFailures;return false;}
            // Interleave sides; scan several inset strips where streets reach the perimeter.
            // No player-centered ring fallback in Endless.
            const FVector Entry=GetActorLocation()-PerimeterBounds.GetCenter();
            const int32 EntrySide=FMath::Abs(Entry.X)>=FMath::Abs(Entry.Y)?(Entry.X>0?1:0):(Entry.Y>0?3:2);
            const float EntryAlong=EntrySide<2?(GetActorLocation().Y-PerimeterBounds.Min.Y)/PerimeterBounds.GetSize().Y:(GetActorLocation().X-PerimeterBounds.Min.X)/PerimeterBounds.GetSize().X;
            const int32 BaseEntry=FMath::Clamp(FMath::FloorToInt(EntryAlong*12),0,11)*4+EntrySide;
            for(int32 I=0;I<48;++I)
            {
                // The map's eight placed entrances seed distinct sides without relying on UObject ID strides.
                const int32 Index=(Sequence*13+BaseEntry+I)%48;
                const int32 Side=Index%4;const float Along=((Index/4)+.5f)/12;
                const float Inset=600.f+((I/16)%3)*1000.f;
                const FVector Low=PerimeterBounds.Min+FVector(Inset,Inset,0),High=PerimeterBounds.Max-FVector(Inset,Inset,0);
                FVector Candidate(0,0,PlayerPoint.Location.Z);
                if(Side<2){Candidate.X=Side==0?Low.X:High.X;Candidate.Y=FMath::Lerp(Low.Y,High.Y,Along);}
                else{Candidate.X=FMath::Lerp(Low.X,High.X,Along);Candidate.Y=Side==2?Low.Y:High.Y;}
                if(SafePoint(Candidate,OutLocation,FVector(500,500,500))){GroupOrigin=LastGroupOrigin=OutLocation;bHasGroupOrigin=true;return true;}
                if(PathQueries>=12)break;
            }
        }
        else
        {
            // Corners leave only a small navigable arc: explore 360 degrees at multiple distances.
            // Reachable sampling is the final fallback for irregular rooms and corridors.
            for(int32 I=0;I<36;++I)
            {
                const float Angle=FMath::DegreesToRadians((Sequence*137+I*137)%360);
                const float Radius=I<12?FMath::FRandRange(Inner,2000.f):FMath::FRandRange(FMath::Max(Inner,2000.f),4500.f);
                const FVector Candidate=PlayerFeet+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Radius;
                if(SafePoint(Candidate,OutLocation,FVector(240,240,400))){GroupOrigin=LastGroupOrigin=OutLocation;bHasGroupOrigin=true;return true;}
                if(PathQueries>=12)break;
            }
            for(int32 I=0;I<12&&PathQueries<12;++I)
            {
                FNavLocation Random;
                if(Nav->GetRandomReachablePointInRadius(PlayerPoint.Location,4500,Random,NavData)&&SafePoint(Random.Location,OutLocation,FVector(100,100,200)))
                {GroupOrigin=LastGroupOrigin=OutLocation;bHasGroupOrigin=true;return true;}
            }
        }
        AllowVisibleFallback=true;PathQueries=0;
        for(int32 I=0;I<32;++I)
        {
            FVector Candidate;
            if(Endless)
            {
                if(!PerimeterBounds.IsValid)break;
                const int32 Side=(Sequence+I)%4;const float Along=((I/4)+.5f)/8.f;const float Inset=800.f+(I%3)*900.f;
                const FVector Low=PerimeterBounds.Min+FVector(Inset,Inset,0),High=PerimeterBounds.Max-FVector(Inset,Inset,0);
                Candidate=Side<2?FVector(Side==0?Low.X:High.X,FMath::Lerp(Low.Y,High.Y,Along),PlayerPoint.Location.Z)
                    :FVector(FMath::Lerp(Low.X,High.X,Along),Side==2?Low.Y:High.Y,PlayerPoint.Location.Z);
            }
            else
            {
                const float Angle=FMath::DegreesToRadians((Sequence*137+I*137)%360);const float Radius=I<16?FMath::Max(Inner+400,1600.f):FMath::Max(Inner+800,2800.f);
                Candidate=PlayerFeet+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Radius;
            }
            if(SafePoint(Candidate,OutLocation,FVector(650,650,700)))
            {GroupOrigin=LastGroupOrigin=OutLocation;bHasGroupOrigin=true;return true;}
            if(PathQueries>=12)break;
        }
        ++SpawnLocationFailures;return false;
    }
    for(int32 I=0;I<SPAWN_LOCATION_TRY_COUNT;++I)
    {
        FVector Point;if(!TryGetRandomNavLocation(Point))continue;
        if(PawnCharacter&&FVector::DistSquared(Point,PawnCharacter->GetActorLocation())<FMath::Square(double(MinPlayerDistance)))continue;
        OutLocation=Point;return true;
    }
    ++SpawnLocationFailures;return false;
}

AMonsterBase* AMonsterSpawnVolume::SpawnOneMonster(const TArray<TSubclassOf<AMonsterBase>>& MonsterClasses, const FVector& SpawnLocation)
{
	//낼 종류를 안 채워뒀으면 낼 것이 없음
	if (MonsterClasses.Num() == 0)
	{
		return nullptr;
	}

	//비어 있는 칸을 뽑을 수 있으므로 채워진 것만 모아서 고름
	//블루프린트에서 배열 칸만 늘리고 클래스를 안 넣어두는 일이 흔함
	TArray<TSubclassOf<AMonsterBase>> ValidClasses;

	for (const TSubclassOf<AMonsterBase>& MonsterClass : MonsterClasses)
	{
		if (MonsterClass)
		{
			ValidClasses.Add(MonsterClass);
		}
	}

	if (ValidClasses.Num() == 0)
	{
		return nullptr;
	}

    if (&MonsterClasses == &BaseMonsters)
    {
        const auto* GI = GetGameInstance<UDreamVeilGameInstance>();
        const float RangedShares[] = { .20f, .30f, .40f, .35f };
        const int32 StageIndex = FMath::Clamp((GI ? GI->GetCurrentLevelNumber() : 1) - 1, 0, 3);
        const auto* Profile=GI?GI->GetBalanceProfile():nullptr;
        const float RangedShare=Profile?FMath::Clamp(GI->IsInEndless()?Profile->EndlessRangedShare:
            (Profile->StageRangedShares.IsValidIndex(StageIndex)?Profile->StageRangedShares[StageIndex]:RangedShares[StageIndex]),0.0f,1.0f):
            (GI && GI->IsInEndless()?.45f:RangedShares[StageIndex]);
        const bool WantRanged = FMath::FRand() < RangedShare;
        TArray<TSubclassOf<AMonsterBase>> Matching;
        for (const auto& Class : ValidClasses)
        {
            const auto* Defaults = Class->GetDefaultObject<AMonsterBase>();
            if ((Defaults->GetAttackType() == EMonsterAttackType::Ranged) == WantRanged) Matching.Add(Class);
        }
        if (Matching.Num()) ValidClasses = MoveTemp(Matching);
    }
	const TSubclassOf<AMonsterBase> ChosenClass = ValidClasses[FMath::RandRange(0, ValidClasses.Num() - 1)];

	//캡슐 절반 높이만큼 띄워서 냄 바닥에 파묻힌 채로 나오면 이동 컴포넌트가 밀어내느라 튕김
	FVector AdjustedLocation = SpawnLocation;

	if (const ACharacter* MonsterDefault = ChosenClass->GetDefaultObject<ACharacter>())
	{
		if (const UCapsuleComponent* DefaultCapsule = MonsterDefault->GetCapsuleComponent())
		{
			AdjustedLocation.Z += DefaultCapsule->GetScaledCapsuleHalfHeight();
		}
	}

	FActorSpawnParameters SpawnParameters;

	//좁은 곳이라 겹쳐도 일단 냄 안 그러면 몬스터가 몰린 웨이브에서 스폰이 통째로 실패함
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	//볼륨이 보는 쪽을 보고 나옴 문 앞에 놓고 화살표를 방 안쪽으로 돌려두면 문에서 걸어나온 것처럼 보임
	//플레이어 쪽을 보게 하지 않는 이유 플레이어가 문 옆에 서 있으면 문을 등지지 않고 옆을 보고 나와서
	//문에서 나왔다는 느낌이 깨짐 어차피 나온 뒤에는 AI가 바로 플레이어 쪽으로 돌림
	//Pitch Roll을 0으로 두는 이유 볼륨을 기울여 놓아도 몬스터가 누운 채로 나오지 않게
	FRotator SpawnRotation = GetActorForwardVector().Rotation();

	SpawnRotation.Pitch = 0.0f;
	SpawnRotation.Roll = 0.0f;

	//낸 몬스터를 등록하거나 스펙을 걸어주는 일은 하지 않음
	//게임모드가 액터 스폰을 지켜보고 있다가 알아서 등록하고 스펙은 몬스터가 스스로 MonsterInit에서 검
	return Cast<AMonsterBase>(UDreamObjectPoolSubsystem::Spawn(GetWorld(), ChosenClass, FTransform(SpawnRotation, AdjustedLocation), nullptr, nullptr, SpawnParameters.SpawnCollisionHandlingOverride));
}

void AMonsterSpawnVolume::BeginPlay()
{
	Super::BeginPlay();

	PlayerPawn = Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

	MaxDifficultyLevel = FMath::Max(MaxDifficultyLevel, 2);

	EliteMonsterMinRate = FMath::Clamp(
		EliteMonsterMinRate,
		0.0f,
		99.0f
	);

	EliteMonsterMaxRate = FMath::Clamp(
		EliteMonsterMaxRate,
		EliteMonsterMinRate + 1.0f,
		100.0f
	);

	DifficultyCurve = FMath::Clamp(
		DifficultyCurve,
		0.1f,
		5.0f
	);
}

//플레이어를 찾아서 들고 있음
AMainPlayerCharacter* AMonsterSpawnVolume::GetPlayerCharacter()
{
	if (!PlayerPawn)
	{
		PlayerPawn = Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	}

	return PlayerPawn;
}

float AMonsterSpawnVolume::GetEliteRate()
{
	const AMainPlayerCharacter* Player = GetPlayerCharacter();

	//플레이어를 아직 못 찾았으면 최소 확률로 둠
	//0을 돌려주면 엘리트가 한 마리도 안 나오는데 그게 기획인지 사고인지 구분이 안 됨
	if (!Player)
	{
		return EliteMonsterMinRate;
	}

	int32 PlayerLevel = Player->GetPlayerLevel();

	float LevelAlpha = FMath::Clamp(
		static_cast<float>(PlayerLevel - 1) / (MaxDifficultyLevel - 1),
		0.0f,
		1.0f
	);
	
	// 게임 난이도가 이 경사를 눕히거나 세움. 쉬움이면 엘리트가 늦게 나오고 어려움이면 빨리 나옴
	// 최소 최대 스폰율은 그대로 둬서 위에 잡아둔 값의 뜻이 바뀌지 않음
	float CurveExponent = DifficultyCurve;

	if (const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>())
	{
		CurveExponent *= DreamVeilGameInstance->GetSpawnCurveScale();
	}

	LevelAlpha = FMath::Pow(LevelAlpha, CurveExponent);

	return FMath::Lerp(EliteMonsterMinRate, EliteMonsterMaxRate, LevelAlpha);
}

// 웨이브 스폰
// 게임모드가 "이번 웨이브에 몇 마리 내라"고 시키면 여기서 간격을 두고 내보냄
// 실제로 한 마리를 만드는 일은 ExecuteSpawnActor가 하므로 그쪽만 고치면 스폰 방식이 바뀜

//게임모드가 주기마다 부르는 스폰 신호
//한 마리씩 내는 절차는 웨이브와 같으므로 SpawnWave를 그대로 씀
//간격을 주기의 절반으로 나눈 이유 다음 신호가 오기 전에 이번 몫을 다 내보내야 밀리지 않음
bool AMonsterSpawnVolume::IsSpawnPointCoolingDown()const
{return GetWorld()&&GetWorld()->GetTimeSeconds()<NextPackRequestTime;}
void AMonsterSpawnVolume::SpawnTick(){SpawnPackNow();}
int32 AMonsterSpawnVolume::SpawnPackNow()
{
    if(MonstersPerSpawnTick<=0||!GetWorld()||IsSpawnPointCoolingDown())return 0;
    NextPackRequestTime=GetWorld()->GetTimeSeconds()+.5f;
    GetWorldTimerManager().ClearTimer(WaveSpawnTimerHandle);
    const int32 Minimum=FMath::Clamp(SpawnGroupMin,2,4);
    PendingSpawnCount=FMath::RandRange(Minimum,FMath::Clamp(SpawnGroupMax,Minimum,4));
    bContinuousPack=true;SpawnOneFromWave();
    ConsecutivePositionFailures=LastSpawnedGroupCount>0?0:ConsecutivePositionFailures+1;
    return LastSpawnedGroupCount;
}

//웨이브 시작 남은 수를 더하고 타이머를 깨움
void AMonsterSpawnVolume::SpawnWave(int32 MonsterCount, float SpawnInterval)
{
    bContinuousPack=false;
	if (MonsterCount <= 0)
	{
		return;
	}

	//앞 웨이브가 아직 다 안 나왔으면 남은 수에 더함 타이머가 이어서 마저 내보냄
	// Saturate deferred work so blocked spawn points cannot queue a later burst.
	PendingSpawnCount = FMath::Min(PendingSpawnCount + MonsterCount, 128);

	//이미 돌고 있으면 그대로 두고 남은 수만 늘어남 타이머를 다시 걸면 간격이 흐트러짐
	if (GetWorldTimerManager().IsTimerActive(WaveSpawnTimerHandle))
	{
		return;
	}

	//0 이하가 들어오면 타이머가 매 프레임 돌아서 최소값으로 막음
	const float SafeInterval = FMath::Max(SpawnInterval, 0.05f);

	//첫 마리도 간격만큼 기다렸다 나옴 웨이브 시작과 동시에 눈앞에 튀어나오지 않게
	GetWorldTimerManager().SetTimer(WaveSpawnTimerHandle, this, &AMonsterSpawnVolume::SpawnOneFromWave, SafeInterval, true);
}

// Consume a pack in one timer callback, preserving the shared alive budget.
void AMonsterSpawnVolume::SpawnOneFromWave()
{
    LastSpawnedGroupCount=0;bHasGroupOrigin=false;CachedGroupEliteCount=INDEX_NONE;bInsideSpawnGroup=true;
    // Never split a normal 2..4 request into a pair followed by a lone straggler.
    const int32 Count=PendingSpawnCount<=4?PendingSpawnCount:(PendingSpawnCount==5?3:4);
    FVector OpenDirection;
    auto* Tactics=GetWorld()->GetSubsystem<UDreamCrowdTacticsSubsystem>();
    const bool Corner=Tactics&&Tactics->IsCornered(GetPlayerCharacter(),OpenDirection);
    for(int32 I=0;I<Count;++I)
    {
        if(const auto* Mode=GetWorld()->GetAuthGameMode<AMainGameModeBase>();Mode&&!Mode->CanSpawnRegularMonster())break;
        bPreferRangedMember=Corner&&LastSpawnedGroupCount==0;
        if(!TrySpawnMonster())continue;
        --PendingSpawnCount;++LastSpawnedGroupCount;
    }
    if(LastSpawnedGroupCount>0)if(auto* Flow=GetWorld()->GetSubsystem<UDreamEncounterDirector>())Flow->PackSpawned();
    bPreferRangedMember=false;bHasGroupOrigin=false;bInsideSpawnGroup=false;CachedGroupEliteCount=INDEX_NONE;
    // Continuous pressure follows the game-mode clock, not a rapid retry/refill loop.
    if(bContinuousPack)PendingSpawnCount=0;
    if(PendingSpawnCount<=0)GetWorldTimerManager().ClearTimer(WaveSpawnTimerHandle);
}
