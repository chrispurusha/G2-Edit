# backdoor.h notes

The longer comments from `backdoor.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `backdoor_enabled()`

The backdoor test-control channel — a file-driven way to drive AND independently verify the
running app. The command surface, the gating environment variable and the reason each command
exists are all documented at the top of backdoor.c.

Only two entry points, both called from the render loop (graphics.c): backdoor_poll() honours one
command per tick, and backdoor_enabled() is what tells that loop to tick at all — an unset
G2_EDIT_BACKDOOR leaves the channel completely inert and the idle loop asleep in glfwWaitEvents().
