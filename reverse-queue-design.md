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
1. Generalize `msgQueue` to a size-agnostic queue in place (forward queue keeps working).
2. **[DONE 2026-07-26]** Add `gResponseQueue` + poll-drain; migrate ONE op as proof — file
   **load** result → surface failure via `show_alert` (closes the silent-failure gap).
3. **[PARTIAL 2026-07-26]** Migrate remaining `check_action_flags` result-blocks onto
   the queue. Done: file **save** (`eRspFileSave`) and **New Patch** (`eRspNewPatch`)
   now post results (writers return status). Still open: the backup/restore/store-peek
   blocks (working code with their own progress UI — retire their bespoke globals later).
4. **[DONE 2026-07-26]** `gDeviceOpInProgress` busy state + dim overlay + input swallow.
5. Lift the generic queue to SynthLib.
6. Adopt in Z1/EmuUtility.

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
