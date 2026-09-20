// Undo is the application UI's, and the chain builder never reaches it from a tool.
#include <stdint.h>
void undo_push_param_change(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t g) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g;
}
