# Reverse (USB→UI) message queue — design & progress

Living design note. Updated as the work proceeds. See also `todo.txt`
("Reverse (USB->UI) message queue + GUI busy state").

## Problem

Every async USB-thread operation reports back to the UI by inventing 2–4 `_Atomic`
globals (a "done" flag, a result string, some context bools) plus a drain block in
`check_action_flags()` (graphics.cpp). Examples today: `gBankBackupComplete` +
`gBankBackupResultMessage` + `gBankBackupIsPerf` + `gBankBackupIsEverything`,
`gSynthSettingsBackupComplete`, `gBankRestoreComplete`, `gStorePeekComplete`,
`gStorePatchResultMessage`. This doesn't scale, has no command↔result correlation,
and the newer USB-thread file ops report failure only to the log (silent to the user).

Goal: one structured channel carrying `{what finished, ok/fail, detail}` from the USB
thread back to the UI thread.

## Core design

Reuse the existing queue mechanism (`tMessageQueue` + `msg_send`/`msg_receive`, a
mutex+condvar linked list). The only asymmetry vs. the forward (UI→USB) queue is the
consumer:

| | Forward (UI→USB) `gCommandQueue` | Reverse (USB→UI) `gResponseQueue` |
|---|---|---|
| Producer | UI thread `msg_send` | USB thread `msg_send` |
| Consumer | USB thread, **blocks** `msg_receive(eRcvWait)` | UI thread, **must not block** |
| Wake | condvar (`semCount`) | `wake_glfw()` → `glfwWaitEvents()` returns |
| Drain | dedicated loop | poll `eRcvPoll` until empty, in `check_action_flags()` |

So the reverse queue is the **same mechanism, poll-drained**. USB thread:
`msg_send(&gResponseQueue, …); call_wake_glfw();`. UI thread, top of `do_graphics_loop`
via `check_action_flags()`:
`while (msg_receive(&gResponseQueue, eRcvPoll, &resp) == EXIT_SUCCESS) dispatch(resp);`

### Data model
The thin slice reuses `tMessageContent` for responses too (no queue generalization
yet — that's Step 1, deliberately deferred). On the response queue the `cmd` field
holds an `eResponseType` (NOT an `eMsgCmd`); the two queues never cross, so the value
spaces are independent. Payloads live in the existing union (`tFileResultData`, …).

### What belongs on the queue — and what doesn't
- **On the queue:** discrete op results/events — file load/save done, bank
  backup/restore done, store-peek result, new-patch done. One message each.
- **Stays an atomic flag:** the *coalescing* dirty-bits — `gotPatchChangeIndication[]`,
  `gotPerfSettingsChangeIndication`, `gReDraw`. N rapid changes must collapse to ONE
  resync; a queue would (wrongly) enqueue N. Do not migrate these.

### GUI busy state (paired half, later step)
One atomic `gDeviceOpInProgress`: UI sets it when enqueuing a whole-slot command,
the matching response clears it; render loop dims the canvas + shows "Loading…/Saving…"
and swallows canvas input while set.

## Commonalisation into SynthLib (the bigger prize)
- Lift the **generic queue** (payload-size-agnostic: opaque fixed-size blob instead of
  a hardcoded `tMessageContent`) into SynthLib.
- G2-Edit instantiates two: `commandQueue` (UI→USB) + `responseQueue` (USB→UI).
- Z1-Edit / EmuUtility have **no** queue today (MIDI thread + UI share plain globals +
  wake). Adopting the same two-queue pattern unifies all three: USB vs MIDI becomes
  just the backend that drains commands and posts responses; the queue+wake contract is
  identical. App-specific payload types stay per-app; only the mechanism is shared.

## Migration plan (each step builds & ships)
1. **[DONE 2026-07-26]** Generalize `msgQueue` to a size-agnostic queue in place (both queues
   keep working). The node is now a flexible-array payload (`tMessage { next; uint8_t payload[] }`);
   `tMessageQueue` carries a `payloadSize` set at `msg_init(q, name, size)`; `msg_send`/`msg_receive`
   take `void*`/`const void*` and byte-copy `payloadSize` bytes (no dependence on `tMessageContent`,
   so the mechanism is SynthLib-liftable as-is). Callers pass a `tMessageContent` (implicit `→ void*`).
   Also renamed the queues to direction-based names: `gCommandQueue → gToUsbThread`,
   `gResponseQueue → gToGuiThread` (owner's suggestion — transport-agnostic, ready for SynthLib).
2. **[DONE 2026-07-26]** Add `gResponseQueue` + poll-drain; migrate ONE op as proof — file
   **load** result → surface failure via `show_alert` (closes the silent-failure gap).
3. **[DONE 2026-07-26]** Migrate the "op finished → alert" result-blocks onto the queue
   as a generic `eRspAlert {title, message}` (via `post_alert_response()`), retiring 15
   `*Complete`/`*ResultMessage`/`gLoadFailed` globals: file save, New Patch, bank backup,
   backup-everything, synth-settings backup, bank restore, restore-everything, store,
   delete, load-failure, synth-settings restore. The 4 **peek→confirm** flows
   (store/delete/load/synth-restore peek) are deliberately **left on their flags** — they
   open bespoke confirm dialogs / auto-proceed rather than showing a plain alert, so
   migrating them just relocates that logic at higher risk for little cleanup. The
   `gBank*IsPerf`/`gBank*IsEverything` progress flags stay (the progress overlays read
   them) — the everything-sweep functions now reset `IsEverything` themselves, since the
   drain block that used to do it is gone.
4. **[DONE 2026-07-26]** `gDeviceOpInProgress` busy state + dim overlay + input swallow.
5. **[DONE 2026-07-26]** Lift the generic queue to SynthLib. Moved the mechanism to
   `SynthLib/src/synthlibQueue.{h,c}` (tMessageQueue / tMessage / eRcv / msg_init / msg_send /
   msg_receive / msg_count) — payload-agnostic, depends only on pthreads + synthlibDefs.h's LOG_*.
   G2-Edit's `msgQueue.h` now `#include`s it and keeps only the app payload (tMessageContent + enums);
   `msgQueue.c` deleted (auto-dropped by the Xcode-16 synced folder; `synthlibQueue.c` auto-added).
6. Adopt in Z1/EmuUtility.

## Progress log addendum
- 2026-07-26 — Step 1 DONE (built, Debug xcodebuild). `msgQueue.{c,h}` made payload-agnostic
  (flexible-array node + `payloadSize`), `msg_send`/`msg_receive` now `void*`; queues renamed
  `gToUsbThread` / `gToGuiThread`. No behaviour change — pure mechanism/naming. Next: Step 5
  (lift the generic queue into SynthLib), then Step 6 (adopt in Z1/EmuUtility).

## Progress log addendum 2
- 2026-07-26 — Step 5 DONE (built, Debug xcodebuild). Queue mechanism lifted to
  `SynthLib/src/synthlibQueue.{h,c}`; G2-Edit `msgQueue.h` slimmed to app payload only + includes it;
  `src/msgQueue.c` deleted. NB: this is a SynthLib submodule change — commit it in SynthLib and advance
  the pin in G2-Edit (and later Z1/EmuUtility). synthlibQueue.c will also compile into Z1/EmuUtility once
  their pins advance; they have no msg_* today so no symbol clash (verify at pin-advance time). Only
  Step 6 (actually USING it in Z1/EmuUtility) remains for the commonalisation payoff.

## Drain refinement (2026-07-26)
The render-loop drain processes **one** response per frame (not a `while` loop): a `while`
could fire two `show_alert`s in one frame and the modal alert is singular, so the second
would clobber the first. It also **skips draining while `alert_dialog_active()`** so an
alert isn't replaced before it's dismissed, and **self-wakes** (`wake_glfw()` when
`msg_count() > 0`) so the next queued response is handled next frame rather than blocking
in `glfwWaitEvents`.

## Future idea (owner, 2026-07-26)
The GUI thread could post to the response queue **for its own consumption**, replacing the
UI-only deferred-action flags (`gShowOpenFileReadDialogue`, `gShowOpenFileWriteDialogue`,
`gNeedFocus`, the 4 peek `*Complete` flags, …) — the response queue becomes a general "UI
work queue" both threads feed, collapsing most of `check_action_flags`'s flag tests into
one drain switch. Fits naturally once the queue is generic (Step 1) / in SynthLib (Step 5).

## Progress log
- 2026-07-26 — design captured. Starting Step 2 (thin vertical slice: file-load result).
- 2026-07-26 — Step 2 DONE (built, Debug xcodebuild; not yet exercised on hardware).
  - `msgQueue.h`: added `eResponseType {eRspFileLoad, eRspFileSave}` + `tFileResultData`
    {result, path} + union member `fileResultData`. On `gResponseQueue`, `tMessageContent.cmd`
    holds an `eResponseType` (documented; command/response queues never cross).
  - `gResponseQueue` (globalVars.h/.c), `msg_init(&gResponseQueue, "response")` in
    `usb_thread_loop` alongside the command queue. Reuses the existing queue machinery
    unchanged (no generalization yet — that's Step 1).
  - USB side: `post_file_result_response()` (usbComms.c) — `msg_send` + `call_wake_glfw()`;
    called from the `eMsgCmdLoadFile` handler with `load_file_to_device`'s retVal.
  - UI side: poll-drain loop at the top of `check_action_flags()` (graphics.cpp) — on a
    failed `eRspFileLoad` shows a "Load Failed" alert naming the file. Runs every render-loop
    iteration; woken by the USB thread's `wake_glfw`.
  - NOTE: `eRspFileSave` reserved but not emitted — `write_database_to_file`/`write_perf_to_file`
    return void today; wiring save-failure reporting needs those to return status (a Step 3 task).
  - To verify on hardware: load a corrupt/truncated `.pch2`/`.prf2` while online → expect the
    "Load Failed" alert (previously silent).
- 2026-07-26 — Steps 3 (partial) + 4 DONE (built, Debug xcodebuild; needs hardware test).
  - Save writers now return status: `write_database_to_file`/`write_perf_to_file` → `int`
    (EXIT_SUCCESS/FAILURE on fopen/alloc failure). Save handlers post `eRspFileSave`; New
    Patch handler posts `eRspNewPatch`. UI drain shows "Save Failed"/"New Patch Failed" on
    failure. Offline save paths keep their void-return behaviour (return value ignored).
  - Busy state: `gDeviceOpInProgress` (int) + `gDeviceOpLabel` (globalVars). `device_op_begin()`
    at each online enqueue (Loading…/Saving…/New Patch…); `device_op_end()` when the matching
    response drains. `render_device_busy_overlay()` dims the canvas (`draw_dialog_background_overlay`
    + `draw_panel_chrome`) with a "Please wait" panel; `mouse_button` swallows canvas clicks while
    busy. Safety net: the render loop ticks (`glfwWaitEventsTimeout(0.05)`) while busy and
    `check_action_flags` force-clears the busy state after 5s so a lost completion (e.g. device
    disconnect mid-op) can't lock the GUI. To verify on hardware: load/save should show the dim
    "Please wait" panel and reject clicks for the ~0.5s op; a bad save path → "Save Failed".
  - STILL DEFERRED: backup/restore/store-peek result-block migration (Step 3 remainder);
    Steps 1, 5, 6.
- 2026-07-26 — Step 3 DONE (built, Debug xcodebuild; needs hardware test). Generic `eRspAlert`
  + `post_alert_response()`; migrated store/delete/load/synth-restore results and the bank
  backup/restore/backup-everything/restore-everything/synth-settings-backup completions; 15
  globals retired from globalVars.{c,h}. Peek→confirm flows left on flags (see plan step 3).
  Drain refined to one-per-frame + alert-gated + self-wake (see above). Verify on hardware:
  every backup/restore/store/delete/load/synth op still shows its correct completion alert,
  the everything-sweep progress titles are right, and no alert is skipped or doubled.
