#include <stdio.h>
#include <string.h>

#include "jcommon/H/jcommon.h"
#include "jparse/H/jparse.h"

/* ashfront: developer tool for the new-Ash front end.
 *   ashfront --parse <file.ash>   parse only */
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s --parse <file.ash>\n", argv[0]);
        return 2;
    }
    char *src = j_read_file(argv[2]);
    if (!src) {
        fprintf(stderr, "error: cannot read %s\n", argv[2]);
        return 1;
    }
    Node *u = jparse_unit(argv[2], src);
    printf("parsed %s: %d types\n", argv[2], u->l2.count);
    return 0;
}
