/*
 * $Id$
 * Copyright (C) 2002-2007, The Perl Foundation.
 */

/*

=head1 NAME

compilers/imcc/symreg.c

=head1 DESCRIPTION

imcc symbol handling

XXX: SymReg stuff has become overused. SymReg should be for symbolic
registers, reg allocation, etc. but we are now using it for extensive
symbol table management. Need to convert much of this over the use Symbol
and SymbolTable (see symbol.h and symbol.c)

=head2 Functions

=over 4

=cut

*/


#include "imc.h"

/* Globals: */
/* Code: */

/* HEADERIZER HFILE: compilers/imcc/symreg.h */

/* HEADERIZER BEGIN: static */

PARROT_WARN_UNUSED_RESULT
PARROT_CAN_RETURN_NULL
static SymReg * _get_sym_typed(
    ARGIN(const SymHash *hsh),
    ARGIN(const char *name),
    int t);

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
static char * add_ns(PARROT_INTERP, NOTNULL(char *name))
        __attribute__nonnull__(1)
        __attribute__nonnull__(2);

PARROT_CANNOT_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
static SymReg* mk_pmc_const_2(PARROT_INTERP,
    NOTNULL(IMC_Unit *unit),
    NOTNULL(SymReg *left),
    NOTNULL(SymReg *rhs))
        __attribute__nonnull__(1)
        __attribute__nonnull__(2)
        __attribute__nonnull__(3)
        __attribute__nonnull__(4);

static void resize_symhash(NOTNULL(SymHash *hsh))
        __attribute__nonnull__(1);

/* HEADERIZER END: static */

/*

=item C<void
push_namespace(NOTNULL(char *name))>

RT#48260: Not yet documented!!!

=cut

*/

void
push_namespace(NOTNULL(char *name))
{
    Namespace * const ns = (Namespace *) mem_sys_allocate(sizeof (*ns));

    ns->parent = _namespace;
    ns->name   = name;
    ns->idents = NULL;
    _namespace = ns;
}

/*

=item C<void
pop_namespace(NULLOK(char *name))>

RT#48260: Not yet documented!!!

=cut

*/

void
pop_namespace(NULLOK(char *name))
{
    Namespace * const ns = _namespace;

    if (ns == NULL) {
        fprintf(stderr, "pop() on empty namespace stack\n");
        abort();
    }

    if (name && strcmp(name, ns->name) != 0) {
        fprintf(stderr, "tried to pop namespace(%s), "
                "but top of stack is namespace(%s)\n", name, ns->name);
        abort();
    }

    while (ns->idents) {
        Identifier * const ident = ns->idents;
        ns->idents               = ident->next;
        free(ident);
    }

    _namespace = ns->parent;
    free(ns);
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CAN_RETURN_NULL
static SymReg *
_get_sym_typed(ARGIN(const SymHash *hsh), ARGIN(const char *name), int t)>

Gets a symbol from the hash

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CAN_RETURN_NULL
static SymReg *
_get_sym_typed(ARGIN(const SymHash *hsh), ARGIN(const char *name), int t)
{
    const int i = hash_str(name) % hsh->size;
    SymReg   *p;

    for (p = hsh->data[i]; p; p = p->next) {
        if (!strcmp(name, p->name) && t == p->set)
            return p;
    }

    return NULL;
}

/* symbolic registers */

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
_mk_symreg(NOTNULL(SymHash* hsh), NOTNULL(char *name), int t)>

Makes a new SymReg from its varname and type

char * name is a malloced string that will
be used if the symbol needs to be created, or
freed if an old symbol is found.
This is a potentially dangerous semantic that
should be changed.

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
_mk_symreg(NOTNULL(SymHash* hsh), NOTNULL(char *name), int t)
{
    SymReg * r = _get_sym_typed(hsh, name, t);

    if (r) {
        free(name);
        return r;
    }

    r = (SymReg *)calloc(1, sizeof (SymReg));

    if (!r) {
        fprintf(stderr, "Memory error at mk_symreg\n");
        abort();
    }

    r->set        = t;
    r->type       = VTREG;
    r->name       = name;
    r->color      = -1;
    r->want_regno = -1;

    _store_symreg(hsh, r);

    return r;
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_symreg(PARROT_INTERP, NOTNULL(char *name), int t)>

RT#48260: Not yet documented!!!

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_symreg(PARROT_INTERP, NOTNULL(char *name), int t)
{
    IMC_Unit * const unit = IMCC_INFO(interp)->last_unit;
    return _mk_symreg(&unit->hash, name, t);
}

/*

=item C<PARROT_MALLOC
PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
char *
symreg_to_str(ARGIN(const SymReg *s))>

Dump a SymReg to a printable format.

=cut

*/

PARROT_MALLOC
PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
char *
symreg_to_str(ARGIN(const SymReg *s))
{
    /* NOTE: the below magic number encompasses all the quoted strings which
     * may be included in the sprintf output */
    /* XXX This needs to check for NULL */
    char * const buf = (char *) malloc(250 + strlen(s->name));
    const int    t   = s->type;

    sprintf(buf, "symbol [%s]  set [%c]  color [" INTVAL_FMT "]  type [",
                 s->name, s->set, s->color);

    if (t & VTCONST)      { strcat(buf, "VTCONST ");      }
    if (t & VTREG)        { strcat(buf, "VTREG ");        }
    if (t & VTIDENTIFIER) { strcat(buf, "VTIDENTIFIER "); }
    if (t & VTADDRESS)    { strcat(buf, "VTADDRESS ");    }
    if (t & VTREGKEY)     { strcat(buf, "VTREGKEY ");     }
    if (t & VTPASM)       { strcat(buf, "VTPASM ");       }
    if (t & VT_CONSTP)    { strcat(buf, "VT_CONSTP ");    }
    if (t & VT_PCC_SUB)   { strcat(buf, "VT_PCC_SUB ");   }
    if (t & VT_FLAT)      { strcat(buf, "VT_FLAT ");      }
    if (t & VT_OPTIONAL)  { strcat(buf, "VT_OPTIONAL ");  }
    if (t & VT_NAMED)     { strcat(buf, "VT_NAMED ");  }
    strcat(buf, "]");

    return buf;
}


/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_temp_reg(PARROT_INTERP, int t)>

RT#48260: Not yet documented!!!

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_temp_reg(PARROT_INTERP, int t)
{
    char buf[128];
    static int temp;
    sprintf(buf, "__imcc_temp_%d", ++temp);
    return mk_symreg(interp, str_dup(buf), t);
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_pcc_sub(PARROT_INTERP, NOTNULL(char *name), int proto)>

RT#48260: Not yet documented!!!

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_pcc_sub(PARROT_INTERP, NOTNULL(char *name), int proto)
{
    IMC_Unit * const unit = IMCC_INFO(interp)->last_unit;
    SymReg   * const r    = _mk_symreg(&unit->hash, name, proto);

    r->type    = VT_PCC_SUB;
    r->pcc_sub = (pcc_sub_t*)calloc(1, sizeof (struct pcc_sub_t));

    return r;
}

/*

=item C<void
add_namespace(PARROT_INTERP, NOTNULL(struct _IMC_Unit *unit))>

add current namespace to sub decl

=cut

*/

void
add_namespace(PARROT_INTERP, NOTNULL(struct _IMC_Unit *unit))
{
    SymReg * const ns = IMCC_INFO(interp)->cur_namespace;

    if (!ns)
        return;

    if (unit->_namespace)
        return;

    if (unit->prev && unit->prev->_namespace == ns)
        unit->_namespace = ns;
    else {
        SymReg * const g = dup_sym(ns);
        SymReg        *r = _get_sym(&IMCC_INFO(interp)->ghash, g->name);

        unit->_namespace = g;
        g->reg           = ns;
        g->type          = VT_CONSTP;

        if (!r || r->type != VT_CONSTP)
            _store_symreg(&IMCC_INFO(interp)->ghash, g);
    }
}

/*

=item C<void
add_pcc_arg(NOTNULL(SymReg *r), NOTNULL(SymReg *arg))>

Add a register or constant to the function arg list

=cut

*/

void
add_pcc_arg(NOTNULL(SymReg *r), NOTNULL(SymReg *arg))
{
    const int n      = r->pcc_sub->nargs;

    r->pcc_sub->args = (SymReg **)realloc(r->pcc_sub->args,
        (n + 1) * sizeof (SymReg *));
    r->pcc_sub->args[n]   = arg;
    r->pcc_sub->arg_flags = (int *)realloc(r->pcc_sub->arg_flags,
        (n+1) * sizeof (int));
    r->pcc_sub->arg_flags[n] = arg->type;

    arg->type &= ~(VT_FLAT|VT_OPTIONAL|VT_OPT_FLAG|VT_NAMED);

    r->pcc_sub->nargs++;
}

/* XXX Why do we have both add_pcc_arg and add_pcc_param? */
/*

=item C<void
add_pcc_param(NOTNULL(SymReg *r), NOTNULL(SymReg *arg))>

RT#48260: Not yet documented!!!

=cut

*/

void
add_pcc_param(NOTNULL(SymReg *r), NOTNULL(SymReg *arg))
{
    add_pcc_arg(r, arg);
}

/*

=item C<void
add_pcc_result(NOTNULL(SymReg *r), NOTNULL(SymReg *arg))>

RT#48260: Not yet documented!!!

=cut

*/

void
add_pcc_result(NOTNULL(SymReg *r), NOTNULL(SymReg *arg))
{
    const int n     = r->pcc_sub->nret;

    r->pcc_sub->ret = (SymReg **)realloc(r->pcc_sub->ret,
        (n + 1) * sizeof (SymReg *));

    r->pcc_sub->ret[n] = arg;

    /* we can't keep the flags in the SymReg as the SymReg
     * maybe used with different flags for different calls */
    r->pcc_sub->ret_flags = (int *)realloc(r->pcc_sub->ret_flags,
        (n + 1) * sizeof (int));

    r->pcc_sub->ret_flags[n] = arg->type;

    arg->type &= ~(VT_FLAT|VT_OPTIONAL|VT_OPT_FLAG|VT_NAMED);

    r->pcc_sub->nret++;
}

/*

=item C<void
add_pcc_multi(NOTNULL(SymReg *r), NULLOK(SymReg *arg))>

RT#48260: Not yet documented!!!

=cut

*/

void
add_pcc_multi(NOTNULL(SymReg *r), NULLOK(SymReg *arg))
{
    const int n       = r->pcc_sub->nmulti;

    r->pcc_sub->multi = (SymReg **)realloc(r->pcc_sub->multi,
        (n + 1) * sizeof (SymReg *));

    r->pcc_sub->multi[n] = arg;
    r->pcc_sub->nmulti++;
}

/*

=item C<void
add_pcc_return(NOTNULL(SymReg *r), NOTNULL(SymReg *arg))>

RT#48260: Not yet documented!!!

=cut

*/

void
add_pcc_return(NOTNULL(SymReg *r), NOTNULL(SymReg *arg))
{
    add_pcc_result(r, arg);
}

/*

=item C<void
add_pcc_sub(NOTNULL(SymReg *r), NOTNULL(SymReg *arg))>

RT#48260: Not yet documented!!!

=cut

*/

void
add_pcc_sub(NOTNULL(SymReg *r), NOTNULL(SymReg *arg))
{
    r->pcc_sub->sub = arg;
}

/*

=item C<void
add_pcc_cc(NOTNULL(SymReg *r), NOTNULL(SymReg *arg))>

RT#48260: Not yet documented!!!

=cut

*/

void
add_pcc_cc(NOTNULL(SymReg *r), NOTNULL(SymReg *arg))
{
    r->pcc_sub->cc = arg;
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_pasm_reg(PARROT_INTERP, NOTNULL(char *name))>

RT#48260: Not yet documented!!!

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_pasm_reg(PARROT_INTERP, NOTNULL(char *name))
{
    SymReg * r = _get_sym(&IMCC_INFO(interp)->cur_unit->hash, name);

    if (!r) {
        r        = mk_symreg(interp, name, *name);
        r->type  = VTPASM;
        r->color = atoi(name + 1);

        if (r->color < 0)
            IMCC_fataly(interp, E_SyntaxError,
                "register number out of range '%s'\n", name);
    }

    return r;
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
char *
_mk_fullname(NULLOK(const Namespace *ns), ARGIN(const char *name))>

RT#48260: Not yet documented!!!

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
char *
_mk_fullname(NULLOK(const Namespace *ns), ARGIN(const char *name))
{
    char * result;

    if (!ns)
        return str_dup(name);

    result = (char *) malloc(strlen(name) + strlen(ns->name) + 3);
    sprintf(result, "%s::%s", ns->name, name);

    return result;
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
char *
mk_fullname(ARGIN(const char *name))>

RT#48260: Not yet documented!!!

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
char *
mk_fullname(ARGIN(const char *name))
{
    return _mk_fullname(_namespace, name);
}

/*

=item C<PARROT_CANNOT_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
SymReg *
mk_ident(PARROT_INTERP, NOTNULL(char *name), int t)>

Makes a new identifier

=cut

*/

PARROT_CANNOT_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
SymReg *
mk_ident(PARROT_INTERP, NOTNULL(char *name), int t)
{
    char   *fullname = _mk_fullname(_namespace, name);
    SymReg *r;

    if (_namespace) {
        Identifier * const ident = (Identifier *)calloc(1,
            sizeof (struct ident_t));

        ident->name        = fullname;
        ident->next        = _namespace->idents;
        _namespace->idents = ident;
    }

    r       = mk_symreg(interp, fullname, t);
    r->type = VTIDENTIFIER;

    free(name);

    if (t == 'P') {
        r->pmc_type = IMCC_INFO(interp)->cur_pmc_type;
        IMCC_INFO(interp)->cur_pmc_type = 0;
    }

    return r;
}

/*

=item C<PARROT_CANNOT_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
SymReg*
mk_ident_ur(PARROT_INTERP, NOTNULL(char *name), int t)>

RT#48260: Not yet documented!!!

=cut

*/

PARROT_CANNOT_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
SymReg*
mk_ident_ur(PARROT_INTERP, NOTNULL(char *name), int t)
{
    SymReg * const r = mk_ident(interp, name, t);
    r->usage        |= U_NON_VOLATILE;

    return r;
}

/*

=item C<PARROT_CANNOT_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
static SymReg*
mk_pmc_const_2(PARROT_INTERP, NOTNULL(IMC_Unit *unit), NOTNULL(SymReg *left), NOTNULL(SymReg *rhs))>

RT#48260: Not yet documented!!!

=cut

*/

PARROT_CANNOT_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
static SymReg*
mk_pmc_const_2(PARROT_INTERP, NOTNULL(IMC_Unit *unit), NOTNULL(SymReg *left), NOTNULL(SymReg *rhs))
{
    /* XXX This always returns NULL.  Probably shouldn't return anything then. */
    SymReg *r[2];
    char   *name;
    int     len;

    if (IMCC_INFO(interp)->state->pasm_file) {
        IMCC_fataly(interp, E_SyntaxError,
                "Ident as PMC constant",
                " %s\n", left->name);
    }

    r[0] = left;

    /* strip delimiters */
    name          = str_dup(rhs->name + 1);
    len           = strlen(name);
    name[len - 1] = '\0';

    free(rhs->name);

    rhs->name     = name;
    rhs->set      = 'P';
    rhs->pmc_type = left->pmc_type;

    switch (rhs->pmc_type) {
        case enum_class_Sub:
        case enum_class_Coroutine:
            r[1]       = rhs;
            rhs->usage = U_FIXUP;
            INS(interp, unit, "set_p_pc", "", r, 2, 0, 1);
            return NULL;
        default:
            break;
    }

    r[1] = rhs;
    INS(interp, unit, "set_p_pc", "", r, 2, 0, 1);

    return NULL;
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_const_ident(PARROT_INTERP,
        NOTNULL(char *name), int t, NOTNULL(SymReg *val), int global)>

Makes a new identifier constant with value val

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_const_ident(PARROT_INTERP,
        NOTNULL(char *name), int t, NOTNULL(SymReg *val), int global)
{
    SymReg *r;

    /*
     * Forbid assigning a string to anything other than a string
     * or PMC constant
     */
    if (t == 'N' || t == 'I') {
        if (val->set == 'S') {
            IMCC_fataly(interp, E_TypeError,
                    "bad const initialisation");
        }
        /* Cast value to const type */
        val->set = t;
    }

    if (global) {
        if (t == 'P') {
            IMCC_fataly(interp, E_SyntaxError,
                    "global PMC constant not allowed");
        }
        r = _mk_symreg(&IMCC_INFO(interp)->ghash, name, t);
    }
    else {
        if (t == 'P') {
            r = mk_ident(interp, name, t);
            return mk_pmc_const_2(interp, IMCC_INFO(interp)->cur_unit, r, val);
        }
        r = mk_ident(interp, name, t);
    }

    r->type = VT_CONSTP;
    r->reg  = val;

    return r;
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
_mk_const(NOTNULL(SymHash *hsh), ARGIN(const char *name), int t)>

Makes a new constant

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
_mk_const(NOTNULL(SymHash *hsh), ARGIN(const char *name), int t)
{
    DECL_CONST_CAST;
    SymReg * const r = _mk_symreg(hsh, (char *)const_cast(name), t);
    r->type          = VTCONST;

    if (t == 'U') {
        /* charset:"string" */
        r->set   = 'S';
        r->type |= VT_ENCODED;
    }

    r->use_count++;

    return r;
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_const(PARROT_INTERP, ARGIN(const char *name), int t)>

RT#48260: Not yet documented!!!

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_const(PARROT_INTERP, ARGIN(const char *name), int t)
{
    SymHash * const h = &IMCC_INFO(interp)->ghash;

    if (!h->data)
        create_symhash(h);

    return _mk_const(h, name, t);
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
static char *
add_ns(PARROT_INTERP, NOTNULL(char *name))>

add namespace to sub if any

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
static char *
add_ns(PARROT_INTERP, NOTNULL(char *name))
{
    int len, l;
    char *ns_name, *p;

    if (!IMCC_INFO(interp)->cur_namespace ||
       (l = strlen(IMCC_INFO(interp)->cur_namespace->name)) <= 2)
        return name;

    /* TODO keyed syntax */
    len     = strlen(name) + l  + 4;
    ns_name = (char*)mem_sys_allocate(len);

    strcpy(ns_name, IMCC_INFO(interp)->cur_namespace->name);
    *ns_name       = '_';
    ns_name[l - 1] = '\0';
    strcat(ns_name, "@@@");
    strcat(ns_name, name);
    mem_sys_free(name);

    p = strstr(ns_name, "\";\"");   /* Foo";"Bar  -> Foo@@@Bar */

    while (p) {
        p[0] = '@';
        p[1] = '@';
        p[2] = '@';
        p    = strstr(ns_name, "\";\")");
    }

    return ns_name;
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
_mk_address(PARROT_INTERP, NOTNULL(SymHash *hsh), NOTNULL(char *name),
            int uniq)>

Makes a new address

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
_mk_address(PARROT_INTERP, NOTNULL(SymHash *hsh), NOTNULL(char *name),
            int uniq)
{
    SymReg * r;

    if (uniq == U_add_all) {
        r = (SymReg *)calloc(1, sizeof (SymReg));
        r->type = VTADDRESS;
        r->name = name;
        _store_symreg(hsh, r);
        return r;
    }

    if (uniq == U_add_uniq_sub)
        name = add_ns(interp, name);

    r = _get_sym(hsh, name);
    if (uniq && r && r->type == VTADDRESS &&
            r->lhs_use_count) {      /* we use this for labels/subs */
        if (uniq == U_add_uniq_label) {
            IMCC_fataly(interp, E_SyntaxError,
                   "Label '%s' already defined\n", name);
        }
        else if (uniq == U_add_uniq_sub)
            IMCC_fataly(interp, E_SyntaxError,
                    "Subroutine '%s' already defined\n", name);
    }

    r       = _mk_symreg(hsh, name, 0);
    r->type = VTADDRESS;

    if (uniq)
        r->lhs_use_count++;

    return r;
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_address(PARROT_INTERP, NOTNULL(char *name), int uniq)>

Eventually make mk_address static

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_address(PARROT_INTERP, NOTNULL(char *name), int uniq)
{
    const int begins_with_underscore = *name == '_';
    SymHash * const h = begins_with_underscore
        ? &IMCC_INFO(interp)->ghash : &IMCC_INFO(interp)->cur_unit->hash;
    SymReg  * const s = _mk_address(interp, h, name, uniq);

    if (begins_with_underscore)
       s->usage |= U_FIXUP;

    return s;
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_sub_label(PARROT_INTERP, NOTNULL(char *name))>

Make and store a new address label for a sub.
Label gets a fixup entry.

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_sub_label(PARROT_INTERP, NOTNULL(char *name))
{
    SymReg * const s = _mk_address(interp, &IMCC_INFO(interp)->ghash,
            name, U_add_uniq_sub);

    s->usage |= U_FIXUP;

    return s;
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_sub_address(PARROT_INTERP, NOTNULL(char *name))>

Make a symbol for a label, symbol gets a fixup entry.

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_sub_address(PARROT_INTERP, NOTNULL(char *name))
{
    SymReg * const s = _mk_address(interp, &IMCC_INFO(interp)->ghash,
            name, U_add_all);

    s->usage |= U_FIXUP;

    return s;
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_local_label(PARROT_INTERP, NOTNULL(char *name))>

Make a local symbol, no fixup entry.

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_local_label(PARROT_INTERP, NOTNULL(char *name))
{
    IMC_Unit * const unit = IMCC_INFO(interp)->last_unit;
    return _mk_address(interp, &unit->hash, name, U_add_uniq_label);
}

/*

=item C<PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_label_address(PARROT_INTERP, NOTNULL(char *name))>

RT#48260: Not yet documented!!!

=cut

*/

PARROT_WARN_UNUSED_RESULT
PARROT_CANNOT_RETURN_NULL
SymReg *
mk_label_address(PARROT_INTERP, NOTNULL(char *name))
{
    IMC_Unit * const unit = IMCC_INFO(interp)->last_unit;
    return _mk_address(interp, &unit->hash, name, U_add_once);
}


/*

=item C<PARROT_MALLOC
PARROT_CANNOT_RETURN_NULL
SymReg *
dup_sym(ARGIN(const SymReg *r))>

link keys to a keys structure = SymReg

we might have

what         op      type        pbc.c:build_key()
--------------------------------------------------
 int const   _kic    VTCONST     no
 int reg     _ki     VTREG       no
 str const   _kc     VTCONST     yes
 str reg     _kc     VTREG       yes

 "key" ';' "key" _kc           -> (list of above)   yes
 "key" ';' $I0   _kc  VTREGKEY -> (list of above)   yes

 The information about which reg should be passed to build_key() is
 in the instruction.

 A key containing a variable has a special flag VTREGKEY
 because this key must be considered for life analysis for
 all the chain members, that are variables.

 An instruction with a keychain looks like this

 e.h. set I0, P["abc";0;I1]

 ins->r[2]  = keychain  'K'
 keychain->nextkey = SymReg(VTCONST) "abc"
             ->nextkey = SymReg(VTCONST) 0
                ->nextkey = SymReg(VTREG), ...->reg = VTVAR I1
                   ->nextkey = 0

 We can't use the consts or keys in the chain directly,
 because a different usage would destroy the ->nextkey pointers
 so these are all copies.
 XXX and currently not freed

=cut

*/

PARROT_MALLOC
PARROT_CANNOT_RETURN_NULL
SymReg *
dup_sym(ARGIN(const SymReg *r))
{
    SymReg * const new_sym = mem_allocate_typed(SymReg);
    STRUCT_COPY(new_sym, r);
    new_sym->name = str_dup(r->name);

    return new_sym;
}

/*

=item C<PARROT_CANNOT_RETURN_NULL
SymReg *
link_keys(PARROT_INTERP, int nargs, NOTNULL(SymReg * keys[]), int force)>

RT#48260: Not yet documented!!!

=cut

*/

PARROT_CANNOT_RETURN_NULL
SymReg *
link_keys(PARROT_INTERP, int nargs, NOTNULL(SymReg * keys[]), int force)
{
    SymReg *key, *keychain;
    int i, any_slice;
    size_t len;
    char *key_str;

    /* namespace keys are global consts - no cur_unit */
    SymHash * const h = IMCC_INFO(interp)->cur_unit ?
        &IMCC_INFO(interp)->cur_unit->hash : &IMCC_INFO(interp)->ghash;

    if (nargs == 0)
        IMCC_fataly(interp, E_SyntaxError,
            "link_keys: hu? no keys\n");

    /* short-circuit simple key unless we've been told not to */
    if (nargs == 1 && !force && !(keys[0]->type & VT_SLICE_BITS))
        return keys[0];

    /* calc len of key_str
     * also check if this is a slice - the first key might not
     * have the slice flag set
     */
    for (i = any_slice = 0, len = 0; i < nargs; i++) {
        len += 1 + strlen(keys[i]->name);
        if (keys[i]->type & VT_SLICE_BITS)
            any_slice = 1;
    }

    if (any_slice && !(keys[0]->type & VT_SLICE_BITS))
        keys[0]->type |= (VT_START_SLICE|VT_END_SLICE);

    key_str  = (char*)mem_sys_allocate(len);
    *key_str = 0;

    /* first look, if we already have this exact key chain */
    for (i = 0; i < nargs; i++) {
        strcat(key_str, keys[i]->name);
        /* TODO insert : to compare slices */
        if (i < nargs - 1)
            strcat(key_str, ";");
    }

    if (!any_slice && (keychain = _get_sym(h, key_str)) != 0) {
        free(key_str);
        return keychain;
    }

    /* no, need a new one */
    keychain       = mem_allocate_zeroed_typed(SymReg);
    keychain->type = VTCONST;

    ++keychain->use_count;

    key = keychain;

    for (i = 0; i < nargs; i++) {
        /* if any component is a variable, we need to track it in
         * life analysis
         */
        if (REG_NEEDS_ALLOC(keys[i]))
            keychain->type |= VTREGKEY;

        key->nextkey = dup_sym(keys[i]);
        key          = key->nextkey;

        /* for registers, point ->reg to the original, needed by
         * life analyses & coloring
         */
        if (REG_NEEDS_ALLOC(keys[i]))
            key->reg = keys[i];
    }

    keychain->name  = key_str;
    keychain->set   = 'K';
    keychain->color = -1;

    _store_symreg(h, keychain);

    return keychain;
}

/*

=item C<void
free_sym(NOTNULL(SymReg *r))>

RT#48260: Not yet documented!!!

=cut

*/

void
free_sym(NOTNULL(SymReg *r))
{
    if (r->pcc_sub) {
        if (r->pcc_sub->args)
            free(r->pcc_sub->args);
        if (r->pcc_sub->arg_flags)
            free(r->pcc_sub->arg_flags);
        if (r->pcc_sub->ret)
            free(r->pcc_sub->ret);
        if (r->pcc_sub->ret_flags)
            free(r->pcc_sub->ret_flags);
        free(r->pcc_sub);
    }

    /* TODO free keychain */
    free(r->name);
    free(r);
}

/*
 * This functions manipulate the hash of symbols.
 * XXX: Migrate to use Symbol and SymbolTable
 *
 */

/*

=item C<void
create_symhash(NOTNULL(SymHash *hash))>

RT#48260: Not yet documented!!!

=cut

*/

void
create_symhash(NOTNULL(SymHash *hash))
{
   hash->data    = (SymReg**)mem_sys_allocate_zeroed(16 * sizeof (SymReg*));
   hash->size    = 16;
   hash->entries = 0;
}

/*

=item C<static void
resize_symhash(NOTNULL(SymHash *hsh))>

RT#48260: Not yet documented!!!

=cut

*/

static void
resize_symhash(NOTNULL(SymHash *hsh))
{
    SymHash nh;
    SymReg *r, *next;
    const int new_size = hsh->size << 1;
    int i;
    SymReg ** next_r;
    int n_next, k;

    nh.data = (SymReg**)mem_sys_allocate_zeroed(new_size * sizeof (SymReg*));
    n_next  = 16;
    next_r  =  (SymReg**)mem_sys_allocate_zeroed(n_next  * sizeof (SymReg*));

    for (i = 0; i < hsh->size; i++) {
        int j = 0;
        for (r = hsh->data[i]; r; r = next) {
            next = r->next;
            /*
             * have to remember all the chained next pointers and
             * clear r->next
             */
            if (j >= n_next) {
                n_next <<= 1;
                next_r = (SymReg **)mem_sys_realloc(next_r,
                    n_next * sizeof (SymReg*));
            }

            r->next = NULL;
            next_r[j++] = r;
        }

        for (k = 0; k < j; ++k) {
            int new_i;
            r              = next_r[k];
            new_i          = hash_str(r->name) % new_size;
            r->next        = nh.data[new_i];
            nh.data[new_i] = r;
        }
    }

    mem_sys_free(hsh->data);
    mem_sys_free(next_r);

    hsh->data = nh.data;
    hsh->size = new_size;
}

/*

=item C<void
_store_symreg(NOTNULL(SymHash *hsh), NOTNULL(SymReg *r))>

Stores a symbol into the hash

=cut

*/

void
_store_symreg(NOTNULL(SymHash *hsh), NOTNULL(SymReg *r))
{
    const int i = hash_str(r->name) % hsh->size;
#if IMC_TRACE_HIGH
    printf("    store [%s]\n", r->name);
#endif
    r->next      = hsh->data[i];
    hsh->data[i] = r;

    hsh->entries++;

    if (hsh->entries >= hsh->size)
        resize_symhash(hsh);
}

/*

=item C<void
store_symreg(PARROT_INTERP, NOTNULL(SymReg *r))>

RT#48260: Not yet documented!!!

=cut

*/

void
store_symreg(PARROT_INTERP, NOTNULL(SymReg *r))
{
    _store_symreg(&IMCC_INFO(interp)->cur_unit->hash, r);
}

/*

=item C<PARROT_CAN_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
SymReg *
_get_sym(ARGIN(const SymHash *hsh), ARGIN(const char *name))>

Gets a symbol from the hash

=cut

*/

PARROT_CAN_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
SymReg *
_get_sym(ARGIN(const SymHash *hsh), ARGIN(const char *name))
{
    SymReg   *p;
    const int i = hash_str(name) % hsh->size;

    for (p = hsh->data[i]; p; p = p->next) {
#if IMC_TRACE_HIGH
        printf("   [%s]\n", p->name);
#endif
        if (!strcmp(name, p->name))
            return p;
    }

    return NULL;
}

/*

=item C<PARROT_CAN_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
SymReg *
get_sym(PARROT_INTERP, ARGIN(const char *name))>

Gets a symbol from the current unit symbol table

=cut

*/

PARROT_CAN_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
SymReg *
get_sym(PARROT_INTERP, ARGIN(const char *name))
{
    return _get_sym(&IMCC_INFO(interp)->cur_unit->hash, name);
}

/*

=item C<PARROT_CAN_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
SymReg *
_find_sym(PARROT_INTERP, NULLOK(const Namespace *nspace),
    NOTNULL(SymHash *hsh), ARGIN(const char *name))>

find a symbol hash or ghash

=cut

*/

PARROT_CAN_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
SymReg *
_find_sym(PARROT_INTERP, NULLOK(const Namespace *nspace),
    NOTNULL(SymHash *hsh), ARGIN(const char *name))
{
    const Namespace * ns;
    SymReg *p;

    for (ns = nspace; ns; ns = ns->parent) {
        char * const fullname = _mk_fullname(ns, name);
        p                     = _get_sym(hsh, fullname);

        free(fullname);

        if (p)
            return p;
    }

    p = _get_sym(hsh, name);

    if (p)
        return p;

    p = _get_sym(&IMCC_INFO(interp)->ghash, name);

    if (p)
        return p;

    return NULL;
}


/*

=item C<PARROT_CAN_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
SymReg *
find_sym(PARROT_INTERP, ARGIN(const char *name))>

RT#48260: Not yet documented!!!

=cut

*/

PARROT_CAN_RETURN_NULL
PARROT_WARN_UNUSED_RESULT
SymReg *
find_sym(PARROT_INTERP, ARGIN(const char *name))
{
    if (IMCC_INFO(interp)->cur_unit)
        return _find_sym(interp, _namespace,
            &IMCC_INFO(interp)->cur_unit->hash, name);

    return NULL;
}


/*

=item C<void
clear_sym_hash(NOTNULL(SymHash *hsh))>

RT#48260: Not yet documented!!!

=cut

*/

void
clear_sym_hash(NOTNULL(SymHash *hsh))
{
    int i;

    if (!hsh->data)
        return;

    for (i = 0; i < hsh->size; i++) {
        SymReg *p;
        for (p = hsh->data[i]; p;) {
            SymReg * const next = p->next;
            free_sym(p);
            p = next;
        }
        hsh->data[i] = NULL;
    }

    mem_sys_free(hsh->data);

    hsh->data    = NULL;
    hsh->entries = 0;
    hsh->size    = 0;
}

/*

=item C<void
debug_dump_sym_hash(NOTNULL(SymHash *hsh))>

RT#48260: Not yet documented!!!

=cut

*/

void
debug_dump_sym_hash(NOTNULL(SymHash *hsh))
{
    int i;

    for (i = 0; i < hsh->size; i++) {
        const SymReg *p;
        for (p = hsh->data[i]; p; p = p->next) {
            fprintf(stderr, "%s ", p->name);
        }
    }
}

/*

=item C<void
clear_locals(NULLOK(struct _IMC_Unit *unit))>

Deletes all local symbols and clears life info

=cut

*/

void
clear_locals(NULLOK(struct _IMC_Unit *unit))
{
    SymHash * const hsh = &unit->hash;
    int i;

    for (i = 0; i < hsh->size; i++) {
        SymReg *p;
        for (p = hsh->data[i]; p;) {
            SymReg * const next = p->next;

            if (unit && p->life_info)
                free_life_info(unit, p);

            free_sym(p);
            p = next;
        }

        hsh->data[i] = NULL;
    }

    hsh->entries = 0;
}

/*

=item C<void
clear_globals(PARROT_INTERP)>

Clear global symbols

=cut

*/

void
clear_globals(PARROT_INTERP)
{
    SymHash * const hsh = &IMCC_INFO(interp)->ghash;

    if (hsh->data)
        clear_sym_hash(hsh);
}


/* utility functions: */

/*

=item C<PARROT_PURE_FUNCTION
unsigned int
hash_str(ARGIN(const char *str))>

RT#48260: Not yet documented!!!

=cut

*/

PARROT_PURE_FUNCTION
unsigned int
hash_str(ARGIN(const char *str))
{
    unsigned long  key = 0;
    const    char *s;

    for (s=str; *s; s++)
        key = key * 65599 + *s;

    return key;
}

/*

=back

=cut

*/

/*
 * Local variables:
 *   c-file-style: "parrot"
 * End:
 * vim: expandtab shiftwidth=4:
 */
