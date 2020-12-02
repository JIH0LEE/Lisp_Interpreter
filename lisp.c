#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lisp.h"

extern FILE *yyin;
int yyparse(struct object_s **env);

object *Nil;
object* Error;
object *Else;
object *Ok;
object *Unspecified;



static object FALSE = {
	.type = BOOL,
	{.bool_val = false},
};
static object TRUE = {
	.type = BOOL,
	{.bool_val = true},
};
static object *Symbol_table;
static bool NOT_END = true;

void eof_handle(void)
{
	NOT_END = false;
}



static object *create_object(int type)
{
	object *obj = calloc(1, sizeof(*obj));
	if (obj == NULL) {
		fprintf(stderr, "calloc fail!\n");
		exit(1);
	}
	obj->type = type;
	return obj;
}

static object *car(object *pair)
{
	return pair->car;
}

static object *cdr(object *pair)
{
	return pair->cdr;
}

#define cadr(obj)	car(cdr(obj))
#define cdar(obj)	cdr(car(obj))
#define caar(obj)	car(car(obj))
#define caddr(obj)	car(cdr(cdr(obj)))

object *cons(object *car, object *cdr)
{
	object *pair = create_object(PAIR);
	pair->car = car;
	pair->cdr = cdr;
	return pair;
}

/* return ((x . y) . a) */
static object *acons(object *x, object *y, object *a)
{
	return cons(cons(x, y), a);
}

static void add_variable(object *env, object *sym, object *val)
{
	env->vars = acons(sym, val, env->vars);
}

object *make_bool(bool val)
{
	/* XXX: Efficient but no safe */
	return val ? &TRUE : &FALSE;
}

object *make_char(char val)
{
	object *obj = create_object(CHAR);
	obj->char_val = val;
	return obj;
}

object *make_string(const char *val)
{
	ssize_t len = strlen(val) - 1; /* ignore the last DOUBLE_QUOTE */
	object *obj = create_object(STRING);
	obj->string_val = malloc(len + 1);
	assert(obj->string_val != NULL);
	if (len > 0)
		strncpy(obj->string_val, val, len);
	obj->string_val[len] = '\0';
	return obj;
}

object *make_fixnum(int64_t val)
{
	object *obj = create_object(FIXNUM);
	obj->int_val = val;
	return obj;
}

object *make_emptylist(void)
{
	return Nil;
}

object *make_quote(object *obj)
{
	return cons(make_symbol("quote"), cons(obj, Nil));
}

object *_make_symbol(const char *name)
{
	assert(name != NULL);
	object *obj = create_object(SYMBOL);
	obj->string_val = strdup(name);
	return obj;
}

object *make_symbol(const char *name)
{
	object *p;
	for (p = Symbol_table; p != Nil; p = p->cdr)
		if (strcmp(name, p->car->string_val) == 0)
			return p->car;
	object *sym = _make_symbol(name);
	Symbol_table = cons(sym, Symbol_table);
	return sym;
}

object *make_function(object *parameters, object *body, object *env)
{
	object *function = create_object(COMPOUND_PROC);
	function->parameters = parameters;
	function->body = body;
	function->env = env;
	return function;
}

static object *lookup_variable_val(object *var, object *env)
{
	object *p, *cell;
	for (p = env; p; p = p->up) {
		for (cell = p->vars; cell != Nil; cell = cell->cdr) {
			object *bind = cell->car;
			if (var == bind->car)
				return bind;
		}
	}
	return NULL;
}

static object *list_of_val(object *args, object *env)
{
	if (args == Nil)
		return Nil;
	return cons(eval(env, car(args)), list_of_val(cdr(args), env));
}

static object *apply(object *env, object *fn, object *args)
{
	object *eval_args;
	if (args != Nil && args->type != PAIR)
		DIE("args must be a list");
	if (fn->type == PRIM) {
		eval_args = list_of_val(args, env);
		return fn->func(env, eval_args);
	} else if (fn->type == KEYWORD) {
		return fn->func(env, args);
	}
	debug_print("error");
	return NULL;
}

object *extend_env(object *vars, object *vals, object *base_env)
{
	object *newenv = make_env(Nil, base_env);
	for (; vars != Nil && vals != Nil; vars = vars->cdr, vals = vals->cdr) {
		if (vars->type == SYMBOL) {
			add_variable(newenv, vars, vals);
			return newenv;
		}
		add_variable(newenv, vars->car, vals->car);
	}
	return newenv;
}

static bool is_the_last_arg(object *args)
{
	return (args->cdr == Nil) ? true : false;
}

object *eval(object *env, object *obj)
{
	object *bind;
	object *fn, *args;
	object *newenv, *newobj;
	object *eval_args;
	if (obj == NULL) return NULL;
	switch (obj->type) {
	case SYMBOL:
		bind = lookup_variable_val(obj, env);
		if (!bind)
			return Nil;
		return bind->cdr;
	case PAIR:
		fn = eval(env, obj->car);
		args = obj->cdr;
		if (fn->type == PRIM || fn->type == KEYWORD) {
			return apply(env, fn, args);
		} else if (fn->type == COMPOUND_PROC) {
			eval_args = list_of_val(args, env);																																	
			newenv = extend_env(fn->parameters, eval_args, fn->env);
			for (newobj = fn->body; !is_the_last_arg(newobj);
			     newobj = newobj->cdr)
				(void)eval(newenv, newobj->car);
			return eval(newenv, newobj->car);
		} else {
			return obj; /* list */
		}
	}
	return obj;
}

static void pair_print(const object *pair)
{
	const object *car_pair = pair->car;
	const object *cdr_pair = pair->cdr;
	object_print(car_pair);
	if (cdr_pair == Nil) {
		return;
	} else if (cdr_pair->type == PAIR) {
		printf(" ");
		pair_print(cdr_pair);
	} else {
		printf(" . ");
		object_print(cdr_pair);
	}
}

void object_print(const object *obj)
{
	if (!obj) return;
	switch (obj->type) {
	case FIXNUM:
		printf("%lld", obj->int_val);
		break;
	case KEYWORD:
		printf("<keyword>");
		break;
	case PRIM:
		printf("<primitive>");
		break;
	case BOOL:
		printf("#%c", obj->bool_val ? 't' : 'f');
		break;
	case CHAR:
		printf("#\\%c", obj->char_val);
		break;
	case STRING:
		printf("\"%s\"", obj->string_val);
		break;
	case SYMBOL:
		printf("%s", obj->string_val);
		break;
	case PAIR:
		printf("(");
		pair_print(obj);
		printf(")");
		break;
	case COMPOUND_PROC:
		printf("<proc>");
		break;
	case ERR:
		printf("Error");
		break;
	case NIL:
		printf("Nil");
		break;
	default:
		if (obj == Ok)
			fprintf(stderr, "; ok");
		else if (obj == Unspecified)
			(void)0;
		else
			printf("type: %d", obj->type);
	}
}

static void add_primitive(object *env, char *name, Primitive *func,
			  object_type type)
{
	object *sym = make_symbol(name);
	object *prim = create_object(type);
	prim->func = func;
	add_variable(env, sym, prim);
}

static object *prim_plus(object *env, object *args)
{
	int64_t ret;
	for (ret = 0; args != Nil; args = args->cdr) {
		if (args->car->type != CHAR && args->car->type != FIXNUM) {
			printf("Invalide Argument:");
			object_print(args);
			printf("\n");
			return Error;
		}
		
		ret += (car(args)->int_val);
	}
	return make_fixnum(ret);
}

static int list_length(object *list)
{
	int len = 0;
	for (;;) {
		if (list == Nil)
			return len;
		list = list->cdr;
		len++;
	}
}

static object *prim_sub(object *env, object *args)
{
	int64_t ret;
	if (list_length(args) == 1)
		return make_fixnum(-car(args)->int_val);
	ret = car(args)->int_val;
	for (args = args->cdr; args != Nil; args = args->cdr) {
		if (args->car->type != CHAR && args->car->type != FIXNUM) {
			printf("Invalide Argument:");
			object_print(args);
			printf("\n");
			return Error;
		}
		ret -= (car(args)->int_val);
	}
	return make_fixnum(ret);
}

static object* prim_mul(object* env, object* args)
{
	int64_t ret;
	for (ret = 1; args != Nil; args = args->cdr) {

		if (args->car->type != CHAR && args->car->type != FIXNUM) {
			printf("Invalide Argument:");
			object_print(args);
			printf("\n");
			return Error;
		}
	ret *= (car(args)->int_val);
}
	return make_fixnum(ret);
}

static object *prim_quotient(object *env, object *args)
{

	if (args->car->type != CHAR && args->car->type != FIXNUM) {
		printf("Invalide Argument:");
		object_print(args->car);
		printf("\n");
		return Error;
	}
	if (cadr(args)->type != CHAR && cadr(args)->type != FIXNUM) {
		printf("Invalide Argument:");
		object_print(cadr(args));
		printf("\n");
		return Error;
	}
	return make_fixnum(args->car->int_val / cadr(args)->int_val);
}

static object *prim_cons(object *env, object *args)
{
	if (car(args)->type != FIXNUM && car(args)->type != SYMBOL) {
		printf("1st argument is not atom:");
		object_print(args->car);
		printf("\n");
		return Error;
	}
	if (cadr(args)->type != PAIR) {
		printf("secend argument is not list:");
		object_print(cadr(args));
		printf("\n");
		return Error;
	}

	return cons(car(args), cadr(args));
}

static object *prim_car(object *env, object *args)
{

	if (car(args)->type != PAIR) {
		printf("Invalide Argument:");
		object_print(args->car);
		printf("\n");
		return Error;
	}
	return caar(args);
}

static object *prim_cdr(object *env, object *args)
{

	if (car(args)->type != PAIR) {
		printf("Invalide Argument:");
		object_print(args->car);
		printf("\n");
		return Error;
	}
	
	return cdar(args);
}
static object* prim_error(object* env, object* args)
{

	return Error;
}
static object* prim_nil(object* env, object* args)
{

	return Nil;
}

static object* prim_remove(object* env, object* args)
{
	object* ret = create_object(PAIR);

	if (car(args)->type != FIXNUM && car(args)->type != SYMBOL) {
		printf("1st argument is not atom:");
		object_print(args->car);
		printf("\n");
		return Error;
	}
	if (cdr(args)->type != PAIR) {
		printf("2nd argument is not list:");
		object_print(cdr(args));
		printf("\n");
		return Error;
	}

	ret = cdr(args);
	ret = ret->car; //ret은 이제 입력 리스트

	if ((ret->cdr == Nil) && ((args->parameters->int_val == car(ret)->int_val)
		|| (args->parameters->bool_val == car(ret)->bool_val)
		|| (args->parameters->char_val == car(ret)->char_val)
		|| (args->parameters->string_val == car(ret)->string_val))) // 리스트의 길이가 1일때 같다면
		return Nil;
	else if (ret->cdr == Nil) //한개밖에 없는데 다르다면 그대로 출력
		return ret;

	while (ret->cdr != Nil) // 리스트의 길이가 2이상일때
	{
		while (args->parameters->int_val == car(ret)->int_val
			|| args->parameters->bool_val == car(ret)->bool_val
			|| args->parameters->char_val == car(ret)->char_val
			|| args->parameters->string_val == car(ret)->string_val) //입력한것이랑 같을때
		{
			ret->car = ret->cdr->car;
			if (ret->cdr->cdr != Nil)
				ret->cdr = ret->cdr->cdr;
			else
			{
				ret->cdr->car = NULL;
				if (args->parameters->int_val == car(ret)->int_val
					|| args->parameters->bool_val == car(ret)->bool_val
					|| args->parameters->char_val == car(ret)->char_val
					|| args->parameters->string_val == car(ret)->string_val)
					ret->car = NULL;
				return  cadr(args);
			}
		}
		ret = ret->cdr;
	}
	return  cadr(args);
}


static object* prim_subst(object* env, object* args)
{
	object* ret = create_object(PAIR);

	ret = cdr(cdr(args));
	ret = ret->car;

	while (ret->cdr != Nil) // 리스트의 길이가 2이상일때
	{
		if (cadr(args)->int_val == car(ret)->int_val
			|| cadr(args)->bool_val == car(ret)->bool_val
			|| cadr(args)->char_val == car(ret)->char_val
			|| cadr(args)->string_val == car(ret)->string_val) //입력한것이랑 같을때
		{
			ret->car = car(args);
		};
		ret = ret->cdr;
	}
	if (cadr(args)->int_val == car(ret)->int_val
		|| cadr(args)->bool_val == car(ret)->bool_val
		|| cadr(args)->char_val == car(ret)->char_val
		|| cadr(args)->string_val == car(ret)->string_val) //입력한것이랑 같을때
	{
		ret->car = car(args);
	}
	return car(cdr(cdr(args)));
}


static object *prim_list(object *env, object *args)
{
	return args;
}

static object *prim_quote(object *env, object *args)
{
	if (list_length(args) != 1)
		DIE("quote");
	return args->car;
}

static object *def_var(object *args)
{
	if (args->car->type == SYMBOL)
		return car(args);
	else /* PAIR */
		return caar(args);
}

static object *def_val(object *args, object *env)
{
	if (car(args)->type == PAIR)
		return make_function(cdar(args), cdr(args), env);
	else
		return eval(env, cadr(args));
}

static void define_variable(object *var, object *val, object *env)
{
	object *cell;
	for (cell = env->vars; cell != Nil; cell = cell->cdr) {
		object *oldvar = cell->car;
		if (var == oldvar->car) {
			oldvar->cdr = val;
			return;
		}
	}
	add_variable(env, var, val);
}


static object *prim_define(object *env, object *args)
{
	define_variable(def_var(args), def_val(args, env), env);
	return Ok;
}
static object* prim_setq(object* env, object* args)
{
	if (car(args)->type != SYMBOL) {
		printf("Invalide Argument:");
		object_print(args->car);
		printf("\n");
		return Error;

	}

	define_variable(def_var(args), def_val(args, env), env);
	return def_val(args, env);
}

static object *prim_lambda(object *env, object *args)
{
	return make_function(car(args), cdr(args), env);
}


static bool is_false(object *obj)
{
	if (obj->type != BOOL)
		return false;
	if (obj->bool_val != false)
		return false;
	return true;
}

static bool is_true(object *obj)
{
	return !is_false(obj);
}

static object *prim_and(object *env, object *args)
{
	object *obj;
	object *ret = &TRUE;
	for (; args != Nil; args = args->cdr) {
		obj = car(args);
		ret = eval(env, obj);
		if (is_false(ret))
			return ret;
	}
	return ret;
}

static object *prim_or(object *env, object *args)
{
	object *obj;
	object *ret = &FALSE;
	for (; args != Nil; args = args->cdr) {
		obj = car(args);
		ret = eval(env, obj);
		if (is_true(ret))
			return ret;
	}
	return ret;
}

static object *prim_begin(object *env, object *args)
{
	object *obj;
	object *ret=NULL;
	for (; args != Nil; args = args->cdr) {
		obj = car(args);
		ret = eval(env, obj);
	}
	return ret;
}

static object *prim_if(object *env, object *args)
{
	object *predicate = eval(env, car(args));
	if (is_true(predicate))
		return eval(env, cadr(args));
	if (is_the_last_arg(args->cdr))
		return Unspecified;
	return eval(env, caddr(args));
}

static bool is_else(object *sym)
{
	return (sym == Else) ? true : false;
}

static object *eval_args_list(object *env, object *args_list)
{
	object *arg;
	for (arg = args_list; !is_the_last_arg(arg); arg = arg->cdr)
		(void)eval(env, arg->car);
	return eval(env, arg->car);
}

static object *prim_cond(object *env, object *args)
{
	object *pairs = args;
	object *predicate;
	for (pairs = args; pairs != Nil; pairs = pairs->cdr) {
		predicate = eval(env, caar(pairs));
		if (!is_the_last_arg(pairs)) {
			if (predicate->bool_val)
				return eval_args_list(env, cdar(pairs));
		} else {
			if (is_else(predicate))
				return eval_args_list(env, cdar(pairs));
			else if (predicate->bool_val)
				return eval_args_list(env, cdar(pairs));
		}
	}
	return Unspecified;
}

static object *prim_is_null(object *env, object *args)
{

	
	
		return make_bool((args->car == Nil) ? true : false);
	
}

static object *prim_is_boolean(object *env, object *args)
{
	return make_bool((args->car->type == BOOL) ? true : false);
}

static object *prim_is_pair(object *env, object *args)
{
	return make_bool((args->car->type == PAIR) ? true : false);
}

static object *prim_is_symbol(object *env, object *args)
{
	return make_bool((args->car->type == SYMBOL) ? true : false);
}

static object *prim_is_number(object *env, object *args)
{
	return make_bool((args->car->type == FIXNUM ||
			args->car->type == FLOATNUM) ? true : false);
}

static object *prim_is_char(object *env, object *args)
{
	return make_bool((args->car->type == CHAR) ? true : false);
}

static object *prim_is_string(object *env, object *args)
{
	return make_bool((args->car->type == STRING) ? true : false);
}

static object *prim_is_eq(object *env, object *args)
{
	object *obj;
	object *first = args->car;
	if (args->car->type != args->cdr->type) {
		return make_bool(false);
	}
	for (; args != Nil; args = args->cdr) {
		obj = args->car;
		if (obj->type != first->type)
			return make_bool(false);
		switch (first->type) {
		case FIXNUM:
			if (obj->int_val != first->int_val)
				return make_bool(false);
			break;
		case BOOL:
			if (obj->bool_val != first->bool_val)
				return make_bool(false);
			break;
		case CHAR:
			if (obj->char_val != first->char_val)
				return make_bool(false);
			break;
		case STRING:
			if (strcmp(obj->string_val, first->string_val))
				return make_bool(false);
			break;
		default:
			if (obj != first)
				return make_bool(false);
		}
	}
	return make_bool(true);
}

static object *prim_is_num_eq(object *env, object *args)
{
	int64_t next;
	int64_t first;
	if (args == Nil)
		return make_bool(true);
	first = args->car->int_val;
	for (args = args->cdr; args != Nil; first = next, args = args->cdr) {
		next = args->car->int_val;
		if (next != first)
			return make_bool(false);
	}
	return make_bool(true);
}

static object *prim_is_num_gt(object *env, object *args)
{
	int64_t next;
	int64_t first;

	if (args->car->type != FIXNUM) {
		printf("1st Argument is invalide:");
		object_print(args->car);
		printf("\n");
		return Error;
	}
	if (args->cdr->car->type != FIXNUM) {
		printf("2st Argument is invalide:");
		object_print(args->car);
		printf("\n");
		return Error;
	}
	
	if (args == Nil)
		return make_bool(true);
	first = args->car->int_val;
	for (args = args->cdr; args != Nil; first = next, args = args->cdr) {
		next = args->car->int_val;
		if (next >= first)
			return make_bool(false);
	}
	return make_bool(true);
}

static object* prim_is_num_lt(object* env, object* args)
{
	int64_t next;
	int64_t first;

	if (args->car->type != FIXNUM) {
		printf("1st Argument is invalide:");
		object_print(args->car);
		printf("\n");
		return Error;
	}
	if (args->cdr->car->type != FIXNUM) {
		printf("2st Argument is invalide:");
		object_print(args->car);
		printf("\n");
		return Error;
	}
	if (args == Nil)
		return make_bool(true);
	first = args->car->int_val;
	for (args = args->cdr; args != Nil; first = next, args = args->cdr) {
		next = args->car->int_val;
		if (next <= first)
			return make_bool(false);
	}
	return make_bool(true);
}
static object* prim_is_num_gteq(object* env, object* args)
{
	int64_t next;
	int64_t first;

	if (args->car->type != FIXNUM) {
		printf("1st Argument is invalide:");
		object_print(args->car);
		printf("\n");
		return Error;
	}
	if (args->cdr->car->type != FIXNUM) {
		printf("2st Argument is invalide:");
		object_print(args->car);
		printf("\n");
		return Error;
	}
	if (args == Nil)
		return make_bool(true);
	first = args->car->int_val;
	for (args = args->cdr; args != Nil; first = next, args = args->cdr) {
		next = args->car->int_val;
		if (next > first)
			return make_bool(false);
	}
	return make_bool(true);
}

static object* prim_is_num_lteq(object* env, object* args)
{
	int64_t next;
	int64_t first;

	if (args->car->type != FIXNUM) {
		printf("1st Argument is invalide:");
		object_print(args->car);
		printf("\n");
		return Error;
	}
	if (args->cdr->car->type != FIXNUM) {
		printf("2st Argument is invalide:");
		object_print(args->car);
		printf("\n");
		return Error;
	}
	if (args == Nil)
		return make_bool(true);
	first = args->car->int_val;
	for (args = args->cdr; args != Nil; first = next, args = args->cdr) {
		next = args->car->int_val;
		if (next < first)
			return make_bool(false);
	}
	return make_bool(true);
}

static object *load_file(const char *filename, object *env)
{
	object *obj;
	FILE *oldin = (yyin == NULL) ? stdin : yyin;
	
	FILE *f = fopen(filename, "r");
	if (f == NULL) {
		fprintf(stderr, "fopen() %s fail!\n", filename);
		return Nil;
	}
	yyrestart(f);
	while (NOT_END) {
		yyparse(&obj);
		eval(env, obj);
	}
	fclose(f);
	
	NOT_END = true;
	yyrestart(oldin);
	return Ok;
}


static object *prim_print(object *env, object *args)
{
	if (args->car->type == STRING)
		printf("%s", args->car->string_val);
	else
		object_print(car(args));
	return Unspecified;
}


static void define_prim(object *env)
{
	add_primitive(env, "define", prim_define, KEYWORD);
	add_primitive(env, "setq", prim_setq, KEYWORD);
	add_primitive(env, "lambda", prim_lambda, KEYWORD);
	add_primitive(env, "and", prim_and, KEYWORD);
	add_primitive(env, "or", prim_or, KEYWORD);
	add_primitive(env, "if", prim_if, KEYWORD);
	add_primitive(env, "cond", prim_cond, KEYWORD);
	add_primitive(env, "quote", prim_quote, KEYWORD);
	add_primitive(env, "+", prim_plus, PRIM);
	add_primitive(env, "-", prim_sub, PRIM);
	add_primitive(env, "*", prim_mul, PRIM);
	add_primitive(env, "/", prim_quotient, PRIM);
	add_primitive(env, "cons", prim_cons, PRIM);
	add_primitive(env, "car", prim_car, PRIM);
	add_primitive(env, "cdr", prim_cdr, PRIM);
	add_primitive(env, "remove", prim_remove, PRIM);
	add_primitive(env, "subst", prim_subst, PRIM);
	add_primitive(env, "list", prim_list, PRIM);
	add_primitive(env, "null", prim_is_null, PRIM);
	add_primitive(env, "boolean?", prim_is_boolean, PRIM);
	add_primitive(env, "pair?", prim_is_pair, PRIM);
	add_primitive(env, "atom", prim_is_symbol, PRIM);
	add_primitive(env, "numberp", prim_is_number, PRIM);
	add_primitive(env, "char?", prim_is_char, PRIM);
	add_primitive(env, "stringp", prim_is_string, PRIM);
	add_primitive(env, "equal", prim_is_eq, PRIM);
	add_primitive(env, "eq?", prim_is_num_eq, PRIM);
	add_primitive(env, ">", prim_is_num_gt, PRIM);
	add_primitive(env, "<", prim_is_num_lt, PRIM);
	add_primitive(env, ">=", prim_is_num_gteq, PRIM);
	add_primitive(env, "<=", prim_is_num_lteq, PRIM);
	add_primitive(env, "Nil", prim_nil, NIL);
	add_primitive(env, "Error", prim_error, ERR);
	add_primitive(env, "print", prim_print, PRIM);
	
}

object *make_env(object *var, object *up)
{
	object *env = create_object(ENV);
	env->vars = var;
	env->up = up;	
	return env;
}

int main(int argc, char **argv)
{
	Nil = create_object(NIL);
	Error = create_object(ERR);
	Else = create_object(OTHER);
	Ok = create_object(OTHER);
	Unspecified = create_object(OTHER);
	object *genv = make_env(Nil, NULL);
	object *obj;
	Symbol_table = Nil;
	define_prim(genv);
	add_variable(genv, make_symbol("else"), Else);
	(void)load_file("lisp1.txt", genv);
	fprintf(stderr, "welcome\n> ");
	while (NOT_END) {
		yyparse(&obj);
		object_print(eval(genv, obj));
		printf("\n> ");
	}
	return 0;
}
