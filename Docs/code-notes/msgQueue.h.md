# msgQueue.h notes

The longer comments from `msgQueue.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `eResponseType`

gToGuiThread is the render loop's work queue: tMessageContent.cmd holds an eResponseType, NOT an
eMsgCmd (the gToUsbThread and gToGuiThread queues never cross, so their value spaces are independent).
Most entries are USB-thread results, but the UI thread also posts to it for its own deferred work
(e.g. "open the file browser" from a menu click, handled in the render loop). See reverse-queue-design.md.

## 2. file scope

directly so the whole CRC+sniff+clear+parse+push (load) / DB-read+
serialise (save) runs on the one thread — no cross-thread DB race. See
eMsgCmdLoadFile / eMsgCmdSavePatchFile / eMsgCmdSavePerfFile handlers.
filePath is unused for eMsgCmdNewPatch; slot is unused for the perf
save and for perf loads (whole-DB, all 4 slots).

## 3. file scope

The generic queue mechanism (tMessageQueue / tMessage / eRcv / msg_init / msg_send / msg_receive /
msg_count) now lives in SynthLib (synthlibQueue.h) — it's payload-agnostic, so it carries this
app's tMessageContent for both queues (gToUsbThread / gToGuiThread) without depending on it. This
header defines only the app-specific message content above.
