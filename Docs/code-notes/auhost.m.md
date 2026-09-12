# auhost.m notes

The longer comments from `auhost.m`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

── A minimal Audio Unit host, for looking at our own editor ────────────────────────────────────

The counterpart to vst3host.mm, and it exists for the one thing auval does not do: auval
instantiates the plug-in, renders it and sends it MIDI, but it never opens the Cocoa editor. That
leaves the whole kAudioUnitProperty_CocoaUI path - our own bundle found by identifier, the view
class loaded out of it by name, the view put into a window somebody else owns - untested by the
one tool that otherwise says "AU VALIDATION SUCCEEDED".

It also renders a few blocks with a note held and reports the peak, so "does it make a sound from
the patch it loaded" is answered in the same command.

WHAT IT PROVES, AND WHAT IT DOES NOT. It proves the component is registered, instantiates, draws
its editor and produces audio. It does NOT prove a real host will accept it - the same warning
vst3host.mm carries. auval is the other half of the answer, and a real host is the last word.

THE COMPONENT MUST BE INSTALLED. Unlike a .vst3, which is a path this could dlopen, an Audio Unit
is found through the system's component registry - so it has to be in
/Library/Audio/Plug-Ins/Components (or the per-user one) and AudioComponentRegistrar has to have
noticed it. After a rebuild: killall -9 AudioComponentRegistrar.

```
Build: ./do-auhost    (see tools/README.md)
```

## 2. in `main()`

STOPPING NEEDS AN EVENT TO LAND ON - the same trap vst3host.mm documents at length.
-[NSApplication stop:] only sets a flag, acted on once the current event finishes
being dispatched, so with the pointer sitting still -run stays blocked. The dummy
event is what releases it.
