# vst3host.mm notes

The longer comments from `vst3host.mm`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

── A minimal VST3 host, for looking at our own editor ──────────────────────────────────────────

Loads a .vst3, instantiates the controller, asks it for its editor view, and puts that view in a
window it owns — which is the one relationship with a plug-in view that matters and the one that
cannot be tested any other way. Optionally screenshots the window and exits, so a rendering
change in the plug-in can be diffed the same way one in the application is.

THIS EXISTED TWICE BEFORE AND WAS LOST TWICE, because both times it was written into a scratchpad
rather than the repository (see todo.md, "the hand-written test host in the scratchpad"). It is
here now for that reason as much as any other.

WHAT IT PROVES, AND WHAT IT DOES NOT. It proves the plug-in loads, instantiates, and that its
editor draws. It does NOT prove a host will accept it: an earlier version of this harness asked
only for IPluginFactory and so never noticed that IPluginFactory2 was absent — which is exactly
what Ableton rejected the plug-in for. It asks for IPluginFactory2 now and reports what it finds,
but the general warning stands. Ableton's own ~/Library/Preferences/Ableton/Live */Log.txt names
a rejection cause precisely and remains the last word.

```
Build: ./do-vst3host    (see tools/README.md)
```

## 2. file scope

IPlugFrame's IID is not instantiated by any of the SDK's *iids.cpp files, because those cover
what a PLUG-IN implements and IPlugFrame is the one interface the HOST implements. Defining it
here is the documented way round that and is why this file, not the build script, ends up owning
it.

## 3. file scope

── The rest of a host: enough to connect the two halves and hand a plug-in its state ────────────

WITHOUT THESE THE HARNESS COULD NOT SEE TWO INSTANCES. The processor tells its controller which
instance it is over IConnectionPoint, with a message only the HOST can create - so a harness that
passed a null context and never connected anything only ever exercised the wrappers' "sole
instance" fallback. Taken from GenBridge's tools/vst3check.cpp, where the same gap let a latency
notification reach a DAW broken.

## 4. in `main()`

OPEN AND SHUT, as a host does every time the user clicks the plug-in's editor button. A view
that does not give back its drawing surface, or leaves a timer running, survives one editor
and fails on the ninth: the Metal backend has eight window slots, and G2 Alike once crashed
Live by drawing into a dead view's layer. Three a second, so a run of forty takes ~13 s.
