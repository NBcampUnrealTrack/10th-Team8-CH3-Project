# LV4 ending credits

## Behavior

- LV4 successful clear (including replay) opens `UEndingCreditsWidget` through `AMainPlayerController::ShowEnding`.
- LV1–3 and Endless retain their previous clear behavior.
- The opaque black full-screen UI automatically scrolls through the four supplied contributors and their responsibilities. Names stay Korean; all other text is English.
- After a 2-second opening hold, the roll scrolls at 42 design units/second. At about 51 seconds it stops on **THANK YOU FOR PLAYING**.
- The world is paused, battle BGM is stopped, and pending inventory/augment/game-over callbacks cannot replace the credits.
- Click **RETURN TO LOBBY**, press Enter, or press Escape at any point to finish.
- `FinishEnding` permits completion once and calls the existing `CompleteCurrentLevel` path for rewards, unlocks, saving and lobby travel. The world stays paused until travel.
- Credits do not auto-dismiss. Progress is committed on exit, as in the project's previous ending hook.

## Safe in-game preview

In a non-Shipping build, press **F8** while playing in the lobby or a level to open the same animated credits without completing a level. **F9** is also bound because Play In Editor normally reserves F8 for eject. Do not change editor keybindings for this test.

The button reads **CLOSE PREVIEW**. Clicking it (or pressing Enter; Escape may be intercepted by PIE) closes the preview, unpauses gameplay and restores game input. Preview does not call completion, save, unlock, reward or map-travel functions and leaves the current BGM untouched. Existing menus/pauses/game-over prevent opening a preview. Shipping builds have no preview shortcut.

## Editing credits

The native UMG layout and copy are in `Source/DreamVeil/Private/EndingCreditsWidget.cpp`.
Existing Pretendard font assets are hard-referenced for packaging; no new texture or external font is needed.
The 1400 × 900 design scales to fit and keeps the exit control visible.
No manual Blueprint binding is required. `EndingWidgetClass` may optionally specify a subclass of `UEndingCreditsWidget`; None or a legacy placeholder uses the native screen.

## Verification

Build `DreamVeilEditor Win64 Development` with Unreal Editor closed after reflected C++ changes.

The read-only commandlet option `-run=DreamVeilUIUpdate -RenderEnding -AllowCommandletRendering` checks the four names, closing text, exit button and scroll offsets at 0/24/60 seconds. It writes real UMG previews at 1920×1080, 1280×720, 1280×1024, and 2560×1080 under `Saved/EndingCreditsPreviews`. It uses an isolated transient world, not a map or save slot.

Gameplay acceptance checks (use a disposable test save):

1. Defeat the LV4 boss while alive: credits cover the HUD, enemies stop and no other menu replaces it.
2. Exit via button, Enter, and Escape in separate runs: lobby opens, input works, rewards persist, Endless is unlocked.
3. Rapidly activate exit: only one completion/travel is requested.
4. Replay LV4: credits appear again without increasing progress beyond LV4.
5. Clear LV1–3 or defeat an Endless boss: no ending credits.
6. Die or time out: existing game-over handling, not credits.
7. Leave the credits open while gameplay is paused: the roll continues and stops on the thank-you; the return button stays accessible.

The render/data check is not a substitute for these in-game travel/save checks.
