#include "target.h"

static KilnTarget current_target = KILN_TARGET_LINUX;

void kiln_set_target(KilnTarget target) {
    current_target = target;
}

KilnTarget kiln_get_target(void) {
    return current_target;
}

static KilnLinkMode current_link_mode = KILN_LINK_STATIC;

void kiln_set_link_mode(KilnLinkMode mode) {
    current_link_mode = mode;
}

KilnLinkMode kiln_get_link_mode(void) {
    return current_link_mode;
}

static int compiling_so = 0;

void kiln_set_compiling_so(int flag) {
    compiling_so = flag;
}

int kiln_is_compiling_so(void) {
    return compiling_so;
}
