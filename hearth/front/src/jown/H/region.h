#ifndef JREGION_H
#define JREGION_H

/* Loop regions (kiln static builds): a loop whose body provably keeps nothing it allocates past the iteration
 * gets its heap rewound at the end of every iteration (mem_region_enter / mem_region_leave in the IR). The rule is
 * in region.c; this only marks loop nodes. */

#include "jparse/H/jast.h"

#define JOWN_REGION_LOOP 0x40000       /* Node.flags of an N_WHILE / N_DO / N_FOR */

int region_loop_ok(Node *loop);
/* sets JOWN_REGION_LOOP on every loop of the method body that qualifies */
void region_mark_loops(Node *body);

/* a for-each body that provably cannot change the size of any list (no call but the harmless ones, no project code):
 * the ConcurrentModificationException check can be left out of the lowering */
int foreach_body_keeps_sizes(Node *body);

#endif
