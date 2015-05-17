#include <stdarg.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>

#define SZ(N) ((N)*sizeof(comp_t *))
enum comp_type { ct_sc, ct_ref, ct_val, ct_alg };
typedef struct comp_t {
	 union { 
		void (*sc)(struct comp_t** result, struct comp_t** args);
		struct comp_t* ref;
		double value;
    unsigned tag;
	 } val;
	 int arity;
	 int applied;
	 enum comp_type type;
   struct comp_t** args;
} comp_t;
comp_t* copy(comp_t* existing)
{
		while (existing->type == ct_ref) existing = existing->val.ref;
    comp_t* new = calloc(1, sizeof(comp_t));
    new->val = existing->val;
    new->arity = existing->arity;
    new->applied = existing->applied;
    new->type = existing->type;
    new->args = calloc(existing->applied, sizeof(comp_t*));
    int i;
    for (i=0; i<existing->applied; ++i)
			new->args[i] = existing->args[i];
		return new;
}
void resize(comp_t* existing, int new_applied)
{
	existing->args = realloc(existing->args, new_applied*sizeof(comp_t*));
}
comp_t* constructor(int tag, int k_args, ...)
{
  va_list args;
  comp_t *r = calloc(1, sizeof(comp_t));
  resize(r, r->applied + k_args);
  r->type = ct_alg;
  r->val.tag = tag;
  va_start(args, k_args);
  for(; k_args>0; k_args--)
    r->args[r->applied++] = va_arg(args, comp_t *);
  va_end(args);
  return r;
}
comp_t* app(comp_t* fun, int k_args, ...)
{
  va_list args;
  comp_t *r = copy(fun);
  resize(r, r->applied + k_args);
  va_start(args, k_args);
  for(; k_args>0; k_args--)
    r->args[r->applied++] = va_arg(args, comp_t *);
  va_end(args);
  return r;
}
comp_t* num(double g)
{
	comp_t* r = calloc(1, sizeof(comp_t));
	r->val.value = g;
	r->type = ct_val;
	return r;
}
extern comp_t sc_Cons;
extern comp_t sc_Nil;
comp_t* str(const char* strg)
{
  const char* t = strg;
  while (*t != 0) ++t;
  --t;
  comp_t* r = &sc_Nil;
  while (t >= strg)
	{
		r = app(&sc_Cons, 2, num(*t), r);
		--t;
	}
	return r;
}
comp_t *eval(comp_t *e);
extern comp_t sc_True;
extern comp_t sc_False;
void fun_3e(comp_t** r, comp_t** args) // >
{
    *r = app( (eval(args[0])->val.value > eval(args[1])->val.value) ? &sc_True : &sc_False, 0);
}
comp_t sc_3e = { fun_3e, 2 };
void fun_3c(comp_t** r, comp_t** args) // <
{
    *r = app( (eval(args[0])->val.value < eval(args[1])->val.value) ? &sc_True : &sc_False, 0);
}
comp_t sc_3c = { fun_3c, 2 };
void fun_3e3d(comp_t** r, comp_t** args) // >=
{
    *r = app( (eval(args[0])->val.value >= eval(args[1])->val.value) ? &sc_True : &sc_False, 0);
}
comp_t sc_3e3d = { fun_3e3d, 2 };
void fun_3c3d(comp_t** r, comp_t** args) // <=
{
    *r = app( (eval(args[0])->val.value <= eval(args[1])->val.value) ? &sc_True : &sc_False, 0);
}
comp_t sc_3c3d = { fun_3c3d, 2 };
void fun_3d3d(comp_t** r, comp_t** args) // ==
{
    *r = app( (eval(args[0])->val.value == eval(args[1])->val.value) ? &sc_True : &sc_False, 0);
}
comp_t sc_3d3d = { fun_3d3d, 2 };
void fun_2a(comp_t** r, comp_t** args) // *
{
    *r = num( (eval(args[0])->val.value * eval(args[1])->val.value) );
}
comp_t sc_2a = { fun_2a, 2 };
void fun_2d(comp_t** r, comp_t** args) // -
{
    *r = num( (eval(args[0])->val.value - eval(args[1])->val.value) );
}
comp_t sc_2d = { fun_2d, 2 };
void fun_25(comp_t** r, comp_t** args) // %
{
    *r = num( ((int)eval(args[0])->val.value % (int)eval(args[1])->val.value) );
}
comp_t sc_25 = { fun_25, 2 };
void fun_2f(comp_t** r, comp_t** args) // /
{
    *r = num( ((int)eval(args[0])->val.value / (int)eval(args[1])->val.value) );
}
comp_t sc_2f = { fun_2f, 2 };
void fun_2b(comp_t** r, comp_t** args) // +
{
    *r = num( (eval(args[0])->val.value + eval(args[1])->val.value) );
}
comp_t sc_2b = { fun_2b, 2 };
// helper: follow redirects until target object found
// may return NULL during eval
comp_t *follow(comp_t *v)
{
    while(v && v->type==ct_ref) v = v->val.ref;
    return v;
}

comp_t *eval(comp_t *e) {
  if(e->type == ct_ref)
    return e->val.ref = eval(e->val.ref); // follow and update
  if(e->type == ct_val || e->type == ct_alg)
    return e;
  comp_t hold = { .val = {.ref = NULL}, .type = ct_ref };
  //hold.wnext = root; root = &hold;
  while(e->type != ct_val && e->type != ct_alg && e->applied >= e->arity) {
    // evaluate and let 'e' redirect to the result
    size_t keep = e->applied - e->arity;
    e->val.sc(&hold.val.ref, e->args);
    hold.val.ref = follow(hold.val.ref);
    if(keep > 0) {
      comp_t *r = hold.val.ref = copy(hold.val.ref);
      resize(r, r->applied + keep);
      memcpy(r->args + r->applied, e->args + e->arity, SZ(keep));
      r->applied += keep;
    }
    e->type = ct_ref;
    e->val.ref = hold.val.ref;
    free(e->args); e->args = NULL;
    e = e->val.ref;
  };
  //assert(root == &hold); root = root->wnext;
  return e;
}
void out(comp_t* r)
{
	//puts("out");
  int i;
  const char* types[] = {"sc","ref","val", "alg"};
   switch (r->type) {
	 default:
   case ct_sc:
   case ct_ref:
			//printf("r %p r->type %s r->val.value %f\n", r, types[r->type], r->val.value);
			r = eval(r);
			out(r);
			break;
	 case ct_val:
	    printf("r %p r->type %s r->val.value %f\n", r, types[r->type], r->val.value);
	    break;
	 case ct_alg:
	    printf("r %p r->type %s r->val.tag %d r->applied %d\n", r, types[r->type], r->val.tag, r->applied);
	    for (i=0; i<r->applied; ++i)
	    {
	      //puts("---");
	      out(r->args[i]);
	    }
	    break;
   }
}
int main(int argc,const char** argv)
{
  extern comp_t sc_main;
  comp_t* r = app(&sc_main, 0);
  eval(r);
  r = follow(r);
	printf("main: ");
	out(r);
}
