#include "DreamVeilUIEdits.h"
#include "EndingCreditsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "RenderingThread.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Modules/ModuleManager.h"

bool RenderEndingCredits()
{
	//Commandlets skip normal Slate startup even when RHI rendering is enabled.
	if (!FSlateApplication::IsInitialized())
	{
		FSlateApplication::InitializeAsStandaloneApplication(
			FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
	}
	//Never loads maps or the player's save slot. Only writes preview PNGs.
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Widget = NewObject<UEndingCreditsWidget>(World);
	Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
	bool bPassed = Widget->Initialize();
	Widget->BuildCredits();
	const TCHAR* Names[] = { TEXT("김다솔"), TEXT("정윤재"), TEXT("이미르"), TEXT("김성민") };
	for (int32 I = 0; I < UE_ARRAY_COUNT(Names); ++I)
	{
		auto* Name = Cast<UTextBlock>(Widget->GetWidgetFromName(*FString::Printf(TEXT("Credits_Name_%d"), I)));
		bPassed &= Name && Name->GetText().ToString() == Names[I];
	}
	bPassed &= Cast<UButton>(Widget->GetWidgetFromName(TEXT("Credits_Return"))) != nullptr;
	auto* Thanks = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Credits_Thanks")));
	bPassed &= Thanks && Thanks->GetText().ToString() == TEXT("THANK YOU FOR PLAYING");
	auto* Scroll = Cast<UScrollBox>(Widget->GetWidgetFromName(TEXT("Credits_Scroll")));
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("EndingCreditsPreviews");
	IFileManager::Get().MakeDirectory(*Directory, true);
	const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	for (const FVector2D Resolution : { FVector2D(1920, 1080), FVector2D(1280, 720), FVector2D(1280, 1024), FVector2D(2560, 1080) })
	{
		//PNG export performs the linear-to-sRGB conversion; don't apply gamma twice.
		auto* Renderer = new FWidgetRenderer(false);
		auto* Target = FWidgetRenderer::CreateTargetFor(Resolution, TF_Bilinear, false);
		for (const float Seconds : { 0.0f, 24.0f, 60.0f })
		{
			Widget->UpdateCreditRoll(Seconds);
			//Flush each frame so offscreen layout/scrolling settles before capture.
			for (int32 Frame = 0; Frame < 3; ++Frame)
			{
				Renderer->DrawWidget(Target, SlateWidget, Resolution, 0.016f);
				FlushRenderingCommands();
			}
			const float Expected = FMath::Clamp((Seconds - 2.0f) * 42.0f, 0.0f, 2050.0f);
			bPassed &= Scroll && FMath::IsNearlyEqual(Scroll->GetScrollOffset(), Expected, 1.0f);
			FBufferArchive PNG;
			const FString Filename = Directory / FString::Printf(TEXT("Roll_%dx%d_%02ds.png"), int32(Resolution.X), int32(Resolution.Y), int32(Seconds));
			bPassed &= FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG) && FFileHelper::SaveArrayToFile(PNG, *Filename);
			UE_LOG(LogTemp, Display, TEXT("ENDING PREVIEW: %s"), *Filename);
		}
		BeginCleanup(Renderer);
	}
	Widget->ReleaseSlateResources(true);
	World->DestroyWorld(false);
	UE_LOG(LogTemp, Display, TEXT("ENDING CREDITS RENDER/DATA: %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
	return bPassed;
}
