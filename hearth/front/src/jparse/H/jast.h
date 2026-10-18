#ifndef JAST_H
#define JAST_H

#include "jcommon/H/jcommon.h"

/* AST for new Ash. One generic node type keeps the checker and lowering compact.
 * Field conventions per kind are listed beside each enum entry. `type` and `sym`
 * are filled by the checker (jtypes) and read by lowering (jlower). */

struct JType;
struct JClass;
struct JMember;
struct JLocal;

enum {
    M_PUBLIC = 1, M_PRIVATE = 2, M_PROTECTED = 4, M_STATIC = 8, M_FINAL = 16,
    M_ABSTRACT = 32, M_NATIVE = 64, M_CONST = 128, M_SEALED = 256, M_DEFAULT = 512,
    M_VARARGS = 1024
};

typedef enum {
    /* ---- types and declarations ---- */
    N_TYPE,       /* name=qualified name or primitive, l1=type args, flags=dims, nullable=? */
    N_WILDCARD,   /* a=bound or NULL, name="extends"/"super"/"" */
    N_TYPEPARAM,  /* name, a=bound */
    N_UNIT,       /* name=package, l1=imports, l2=type decls */
    N_IMPORT,     /* name=dotted name, flags=1 for wildcard `.*` */
    N_CLASS, N_INTERFACE, N_ENUM, N_RECORD,
                  /* name, flags=modifiers, l1=members, l2=type params, l3=extends(iface)/implements,
                   * l4=enum constants / record components (N_PARAM), l5=permits, a=superclass type */
    N_FIELD,      /* name, flags, a=type, b=init, l1=annotations */
    N_METHOD,     /* name, flags, a=return type, l1=params, b=body, l2=type params, l3=throws, l4=annos */
    N_CTOR,       /* like N_METHOD with a=NULL */
    N_INIT,       /* static initializer: b=body, flags */
    N_PARAM,      /* name, a=type (NULL for implicit lambda params), flags (M_FINAL, M_VARARGS) */
    N_ENUMCONST,  /* name, l1=args */
    N_ANNO,       /* name, str=optional argument */
    /* ---- statements ---- */
    N_BLOCK,      /* l1 */
    N_SEQ,        /* l1; declarations sharing a scope (int a, b;) */
    N_LOCAL,      /* name, a=type (NULL for var), b=init, flags */
    N_EXPRSTMT,   /* a */
    N_IF,         /* a cond, b then, c else */
    N_WHILE,      /* a cond, b body */
    N_DO,         /* a cond, b body */
    N_FOR,        /* l1 init, a cond, l2 update, b body */
    N_FOREACH,    /* a=type (NULL for var), name, b=iterable, c=body */
    N_RETURN,     /* a */
    N_BREAK, N_CONTINUE, N_EMPTY,
    N_THROW,      /* a */
    N_TRY,        /* b block, l1 catches, c finally */
    N_CATCH,      /* l1 types, name, b block */
    N_SWITCH,     /* a selector, l1 cases, flags=1 when used as expression */
    N_CASE,       /* l1 labels (exprs / N_TYPEPATTERN), flags=1 default, str="->" for arrow form,
                   * l2=body statements (colon form), b=arrow body (expr, block or throw) */
    N_TYPEPATTERN,/* a=type, name=binding */
    /* ---- expressions ---- */
    N_LIT_INT, N_LIT_LONG, N_LIT_FLOAT, N_LIT_DOUBLE, N_LIT_CHAR, N_LIT_STRING,
    N_LIT_BOOL, N_LIT_NULL,  /* name=text */
    N_NAME,       /* name */
    N_FIELDACC,   /* a target, name */
    N_CALL,       /* a target (NULL = unqualified), name, l1 args, flags=1 when target is `super` */
    N_CTORCALL,   /* name="this"/"super", l1 args */
    N_NEW,        /* a type, l1 args */
    N_NEWARRAY,   /* a elem type, l1 dim exprs, flags=total dims, b=initializer */
    N_ARRAYINIT,  /* l1 */
    N_INDEX,      /* a, b */
    N_UNARY,      /* name op, a */
    N_POSTFIX,    /* name op, a */
    N_BINARY,     /* name op, a, b */
    N_ASSIGN,     /* name op ("=", "+=", ...), a, b */
    N_COND,       /* a ? b : c */
    N_CAST,       /* a type, b expr */
    N_INSTANCEOF, /* a expr, b type, name=binding or NULL */
    N_LAMBDA,     /* l1 params, b body (expr or block) */
    N_METHODREF,  /* a target, name */
    N_THIS,
    N_SUPER       /* only as call/field target */
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    int line, col;
    const char *name;
    const char *str;
    int flags;
    int nullable; /* N_TYPE: trailing `?` */
    Node *a, *b, *c;
    JList l1, l2, l3, l4, l5;
    const char *path;
    /* filled by the checker */
    struct JType *type;
    int rk;     /* resolution kind (jtypes.h: R_*) */
    void *sym;
    void *sym2;
};

Node *node_new(NodeKind kind, const char *path, int line, int col);

#endif
