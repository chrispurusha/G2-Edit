# usbLog.c notes

The longer comments from `usbLog.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. in `usb_log_open()`

APPEND, not truncate. This used to open "w", so relaunching the app destroyed the previous
session's capture — which is precisely the wrong behaviour when the thing being captured is a
fault that only shows on a particular device state and may not survive being reproduced twice.
Each session is separated by the marker below. Grows without bound; this is a temporary
diagnostic (see ENABLE_USB_LOG in defs.h) and the file is meant to be deleted afterwards.
