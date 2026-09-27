#pragma once

#include "CoreMinimal.h"
#include "InteractableActorBase.h"
#include "BedActor.generated.h"

class UUserWidget;
class AMainPlayerController;

UCLASS()
class DREAMVEIL_API ABedActor : public AInteractableActorBase
{
	GENERATED_BODY()

protected:
	// BP에서 스테이지 기능을 연결할 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void OnBedInteracted(AActor* Interactor);

	//침대를 눌렀을 때 먼저 뜨는 확인창 꿈에 들어갈지 예 취소로 묻는 화면
	//예 버튼은 OpenDreamSelect를 취소 버튼은 CloseBedMenu를 부르게 연결할 것
	//비워두면 확인을 건너뛰고 바로 꿈 선택창이 뜸
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|UI")
	TSubclassOf<UUserWidget> ConfirmWidgetClass;

	//들어갈 꿈을 고르는 창 L1~L4와 Endless 버튼이 들어감
	//각 버튼은 게임 인스턴스의 OpenLevelByNumber나 OpenEndless를 부르면 됨
	//어느 꿈이 열렸는지 깼는지는 IsLevelUnlocked IsLevelCleared IsEndlessUnlocked로 물어볼 것
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|UI")
	TSubclassOf<UUserWidget> DreamSelectWidgetClass;

public:
	ABedActor();

	// 침대 상호작용 처리
	virtual void Interact_Implementation(AActor* Interactor) override;

	//꿈 선택창을 엶 확인창의 예 버튼이 부를 것
	//확인창을 닫고 여는 일을 컨트롤러가 알아서 하므로 두 창이 겹치지 않음
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OpenDreamSelect();

	//열려 있는 침대 관련 창을 닫음 확인창의 취소 버튼과 선택창의 닫기 버튼이 같이 씀
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void CloseBedMenu();

private:
	//상호작용한 플레이어의 컨트롤러 창을 열고 닫는 일은 전부 컨트롤러가 함
	//침대가 위젯을 직접 만들지 않는 이유 입력 모드 전환과 마우스 커서 처리가 컨트롤러에 이미 있음
	//여기서 또 만들면 침대로 연 창만 입력 처리가 달라져서 W를 누른 채로 열면 계속 걸어감
	AMainPlayerController* GetInteractingController(AActor* Interactor) const;

	//마지막으로 침대를 쓴 플레이어의 컨트롤러 예 버튼을 눌렀을 때 어느 컨트롤러에 창을 띄울지 기억해둠
	TWeakObjectPtr<AMainPlayerController> CachedController;
};
