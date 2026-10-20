#ifndef JREGION_H
#define JREGION_H

/* Loop regions (kiln static builds): a loop whose body provably keeps nothing it allocates past the iteration
 * gets its heap rewound at the end of every iteration (mem_region_enter / mem_region_leave in the IR). The rule is
 * in region.c; this only marks loop nodes. */

#include "jparse/H/jast.h"

#define JOWN_REGION_LOOP 0x40000       /* Node.flags of an N_WHILE / N_DO / N_FOR */
#define JOWN_REGION_BODY 0x80000       /* Node.flags of the body block of a loop lowered with a region: its end frees nothing, the rewind does */

int region_loop_ok(Node *loop);
/* sets JOWN_REGION_LOOP on every loop of the method body that qualifies */
void region_mark_loops(Node *body);

/* a for-each (over the expression `iter`) whose body provably cannot change the size of the list it walks: the
 * ConcurrentModificationException check can be left out of the lowering. Read the rules in region.c. */
int foreach_body_keeps_sizes(Node *iter, Node *body);

#endif
