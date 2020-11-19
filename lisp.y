%{
#include "lisp.tab.h"
#include <stdio.h>
int yylex();
void yyerror(char *);
%}

%union
{
   double dval;
   char *sval;
   struct ast_node *astNode;
   struct symbol_ast_node* symNode;
};

%token <sval> FUNC
%token <sval> TYPE
%token <sval> SYMBOL
%token <dval> NUMBER
%token LPAREN RPAREN EOL QUIT LET COND LETLIST

%type <astNode> s_expr


%%

program:/* empty */ {
                     //  printf("> ");
                    }
        | program s_expr EOL
          {
          //printf("yacc: program expr\n");
          //printf("\n%lf", eval($2)->value);
          //freeNode($2);
          //printf("\n> ");
          }
          ;

s_expr:	element
		{
		
		}
        | LPAREN FUNC s_expr RPAREN
        {
			
          //$$ = function($2, $3, 0);
        }
        | LPAREN FUNC s_expr s_expr RPAREN
        {
		
          //$$ = function($2, $3, $4);
        }
        | LPAREN COND s_expr s_expr s_expr RPAREN
        {
			
          //$$ = condition($3, $4, $5);
        }
        | QUIT
        {
          //exit(0);
        }

        | error
        {
        }
        ;

element: 
		LETLIST value
		{};
		|LPAREN element value RPAREN
		{};
		|LPAREN value RPAREN
		{};
		|LETLIST LPAREN element value RPAREN
		{};
		|value
		{};
value:	NUMBER
        {
          //$$ = number($1);
        }

        |SYMBOL
        {
          //$$ = symbol($1);
        }

%%
int main()
{
  printf("> "); 
  yyparse();
  return 0;
}
void yyerror(char *s)
{
  fprintf(stderr, "%s\n", s);
}