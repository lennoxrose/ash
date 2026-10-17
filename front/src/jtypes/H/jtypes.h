#ifndef JTYPES_H
#define JTYPES_H

#include "jparse/H/jast.h"

/* Types, symbols and the checker API for new Ash.
 * Generics are erased at runtime but fully tracked here. Wildcards are approximated:
 * `? extends X` and `? super X` become X, a bare `?` becomes Object. */

typedef struct JType JType;
typedef struct JClass JClass;
typedef struct JField JField;
typedef struct JMethod JMethod;
typedef struct JTVar JTVar;
typedef struct JLocal JLocal;

typedef enum {
    TY_VOID, TY_BOOL, TY_BYTE, TY_SHORT, TY_CHAR, TY_INT, TY_LONG, TY_FLOAT, TY_DOUBLE,
    TY_NULL, TY_CLASS, TY_ARRAY, TY_TVAR, TY_ERROR
} TyKind;

struct JTVar {
    const char *name;
    JType *bound; /* NULL = Object */
    JClass *owner_cls;
    JMethod *owner_m;
};

struct JType {
    TyKind kind;
    JClass *cls;
    JType **args;
    int nargs;
    JType *elem;
    JTVar *tv;
    int nullable;
};

typedef struct {
    const char *name;
    JType *type;
} JParam;

struct JField {
    const char *name;
    JType *type;
    int flags;
    JClass *cls;
    Node *decl;
    int ordinal;     /* index among instance/static fields */
    const char *lname; /* lowered name */
    const char *tmpl;  /* @Lower template for native constants */
    int is_enum_const;
};

struct JMethod {
    const char *name;
    int flags;
    JClass *cls;
    JType *ret;
    JList params;  /* JParam* */
    JList tparams; /* JTVar* */
    JList throws;  /* JType* */
    Node *decl;
    const char *tmpl; /* @Lower template, NULL for compiled methods */
    int is_ctor;
    int varargs;
    int id;
    const char *lname;
    int has_override_anno;
    int is_record_accessor;
    int synthetic; /* generated (record members, enum values()) */
};

struct JClass {
    const char *name;
    const char *qname;
    const char *pkg;
    const char *mangled;
    NodeKind kind;
    int flags;
    Node *decl;
    const char *path;
    Node *unit;
    JClass *outer;
    JType *super;
    JList ifaces;     /* JType* */
    JList tparams;    /* JTVar* */
    JList fields;     /* JField* */
    JList methods;    /* JMethod* (no ctors) */
    JList ctors;      /* JMethod* */
    JList nested;     /* JClass* */
    JList permits;    /* JClass* */
    JList subclasses; /* JClass* direct subtypes (for devirtualization / sealed) */
    JList enum_names; /* const char* in declaration order */
    JType *self_type; /* C<T1..Tn> */
    int header_done;
    int members_done;
    int checked;
    int native_value; /* represented by a primitive Ash value: String, boxes, arrays */
    int order;        /* load order */
    JMethod *sam;     /* cached single abstract method; sam_done marks the cache as valid */
    int sam_done;
};

/* resolution kinds stored in Node.rk */
enum {
    R_NONE = 0, R_LOCAL, R_FIELD, R_STATIC_FIELD, R_CLASS, R_PACKAGE, R_ARRAYLEN,
    R_METHOD, R_STATIC_METHOD, R_SUPER_METHOD, R_CTOR, R_THIS_FIELD, R_ENUM_CONST,
    R_CAPTURED_THIS
};

/* ---- types (types.c) ---- */
JType *ty_prim(TyKind k);
JType *ty_null(void);
JType *ty_error(void);
JType *ty_class(JClass *c, JType **args, int nargs);
JType *ty_array(JType *elem);
JType *ty_tvar(JTVar *tv);
JType *ty_with_null(JType *t, int nullable);
int ty_is_numeric(JType *t);  /* byte..double (incl. char) */
int ty_is_integral(JType *t);
int ty_is_prim(JType *t);
int ty_is_ref(JType *t);
int ty_same(JType *a, JType *b);
const char *ty_str(JType *t);
JType *ty_unbox(JType *t); /* Integer -> int, else same */
JType *ty_box(JType *t);   /* int -> Integer, else same */
JType *ty_subst(JType *t, JList *tvars, JType **args, int nargs);
JType *ty_erase_bound(JType *t);
JType *ty_as_super(JType *t, JClass *target); /* parameterization of target seen from t, or NULL */
int ty_is_subclass(JClass *c, JClass *target);
int ty_assignable(JType *from, JType *to, int allow_null_mismatch);
JType *ty_binary_numeric(JType *a, JType *b);
int ty_is_string(JType *t);
int ty_is_object(JType *t);
int ty_is_checked_exception(JType *t);

/* ---- registry / loading (classes.c) ---- */
void jt_add_root(const char *dir);
void jt_init_default_roots(const char *entry_path, const char *entry_pkg);
JClass *jt_load_unit(const char *path, const char *src); /* registers every type in the file */
JClass *jt_find_class(const char *qname);                  /* loads lazily from roots; NULL if absent */
JClass *jt_well_known(const char *qname);                  /* aborts when missing (stdlib bug) */
JType *jt_resolve_type(Node *tnode, JClass *ctx, JMethod *mctx, int report);
void jt_complete(JClass *c);   /* resolve header and member signatures */
JField *jt_find_field(JClass *c, const char *name);
void jt_collect_methods(JClass *c, const char *name, JList *out); /* hierarchy incl. interfaces, Object */
JMethod *jt_find_sam(JClass *c);
int jt_all_classes(JClass ***out);
int jt_is_functional(JType *t);
JClass *jt_object(void);
JClass *jt_string(void);
JType *jt_string_type(void);
JType *jt_object_type(void);

/* ---- checker (check_*.c) ---- */
void jt_check_all(void);
void jt_check_class(JClass *c);

void cerr(Node *n, const char *fmt, ...);

#endif
