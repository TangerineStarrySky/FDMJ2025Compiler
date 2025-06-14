%skeleton "lalr1.cc"
%require  "3.2"
%language "c++"
%locations

%code requires {
  #define DEBUG
  #undef DEBUG
  #include <iostream>
  #include <vector>
  #include <algorithm>
  #include <string>
  #include <variant>
  #include "ast_location.hh"
  #include "ASTLexer.hh"
  #include "ASTheader.hh"
  #include "FDMJAST.hh"

  using namespace std;
  using namespace fdmj;
}

%define api.namespace {fdmj}
%define api.parser.class {ASTParser}
%define api.value.type {AST_YYSTYPE}
%define api.location.type {ast_location}

%define parse.error detailed
%define parse.trace

%header
%verbose

%parse-param {ASTLexer &lexer}
%parse-param {const bool debug}
%parse-param {AST_YYSTYPE* result}

%initial-action
{
    #if YYDEBUG != 0
        set_debug_level(debug);
    #endif
};

%code {
    namespace fdmj 
    {
        template<typename RHS>
        void calcLocation(location_t &current, const RHS &rhs, const std::size_t n);
    }
    
    #define YYLLOC_DEFAULT(Cur, Rhs, N) calcLocation(Cur, Rhs, N)
    #define yylex lexer.yylex
    Pos *p;
}

//terminals with no value 
%token PUBLIC INT MAIN RETURN 
//terminals with value
%token<i> NONNEGATIVEINT
%token<s> IDENTIFIER
%token '(' ')' '[' ']' '{' '}' '=' ',' ';' '.' 
%token ADD MINUS TIMES DIVIDE EQ NE LT LE GT GE AND OR NOT

%token TRUE FALSE
%token LENGTH GETINT GETCH GETARRAY PUTINT PUTCH PUTARRAY STARTTIME STOPTIME
%token THIS
%token CLASS EXTENDS
%token IF ELSE WHILE CONTINUE BREAK

//non-terminals, need type information only (not tokens)
%type <program> PROG 
%type <mainMethod> MAINMETHOD 
%type <stm> STM
%type <stmList> STMLIST
%type <exp> EXP
%type <expList> EXPLIST
%type <expList> EXPREST

%type <idExp> ID 

%type <classDecl> CLASSDECL
%type <classDeclList> CLASSDECLLIST
%type <varDecl> VARDECL
%type <varDeclList> VARDECLLIST
%type <methodDecl> METHODDECL
%type <methodDeclList> METHODDECLLIST

%type <formalList> FORMALLIST
%type <formalList> FORMALREST
%type <intExp> CONST
%type <intExpList> CONSTLIST
%type <intExpList> CONSTREST
%type <type> TYPE



%start PROG
%expect 0

%right '='
%left OR
%left AND
%left EQ NE
%left LT LE GT GE
%left ADD MINUS
%left TIMES DIVIDE
%right NOT UMINUS
%right ARRAY_ACCESS 
%right METHOD_CALL
%left '.' '(' ')' '[' ']' 
%precedence THEN
%precedence ELSE


%%

PROG: MAINMETHOD CLASSDECLLIST
  { 
#ifdef DEBUG
    cerr << "Program" << endl;
#endif
    result->root = new Program(p, $1, $2);
  }
  ;
MAINMETHOD: PUBLIC INT MAIN '(' ')' '{' VARDECLLIST STMLIST '}'
  {
#ifdef DEBUG
    cerr << "MainMethod" << endl;
#endif
    $$ = new MainMethod(p, $7, $8) ;
  }
  ;

VARDECLLIST: // empty
  {
#ifdef DEBUG
    cerr << "VARDECLLIST empty" << endl;
#endif
    $$ = new vector<VarDecl*>();
  }
  |
  VARDECL VARDECLLIST
  {
#ifdef DEBUG
    cerr << "VARDECL VARDECLLIST" << endl;
#endif
    vector<VarDecl*> *v = $2;
    v->push_back($1);
    rotate(v->begin(), v->end() - 1, v->end());
    $$ = v;
  }
  ;

VARDECL: CLASS ID ID ';'
  {
    Pos *p1 = new Pos(@1.sline, @1.scolumn, @1.eline, @1.ecolumn);
    $$ = new VarDecl(p, new Type(p1, $2), $3);
  }
  |
  INT ID ';'
  {
    Pos *p1 = new Pos(@1.sline, @1.scolumn, @1.eline, @1.ecolumn);
    $$ = new VarDecl(p, new Type(p1), $2);
  }
  |
  INT ID '=' CONST ';'
  {
    Pos *p1 = new Pos(@1.sline, @1.scolumn, @1.eline, @1.ecolumn);
    $$ = new VarDecl(p, new Type(p1), $2, $4);
  }
  |
  INT '[' ']' ID ';'
  {
    Pos *p1 = new Pos(@1.sline, @1.scolumn, @1.eline, @1.ecolumn);
    $$ = new VarDecl(p, new Type(p1, new IntExp(p1, 0)), $4);  // ???
  }
  |
  INT '[' ']' ID '=' '{' CONSTLIST '}' ';'
  {
    Pos *p1 = new Pos(@1.sline, @1.scolumn, @1.eline, @1.ecolumn);
    $$ = new VarDecl(p, new Type(p1, new IntExp(p1, 0)), $4, $7);  //!???
  }
  |
  INT '[' NONNEGATIVEINT ']' ID ';'
  {
    Pos *p1 = new Pos(@1.sline, @1.scolumn, @1.eline, @1.ecolumn);
    Pos *p2 = new Pos(@3.sline, @3.scolumn, @3.eline, @3.ecolumn);
    $$ = new VarDecl(p, new Type(p1, new IntExp(p2, $3)), $5);  // ???
  }
  |
  INT '[' NONNEGATIVEINT ']' ID '=' '{' CONSTLIST '}' ';'
  {
    Pos *p1 = new Pos(@1.sline, @1.scolumn, @1.eline, @1.ecolumn);
    Pos *p2 = new Pos(@3.sline, @3.scolumn, @3.eline, @3.ecolumn);
    $$ = new VarDecl(p, new Type(p1, new IntExp(p2, $3)), $5, $8);  //???
  }
  ;

CONST: NONNEGATIVEINT
  {
    $$ = new IntExp(p, $1);
  }
  |
  MINUS NONNEGATIVEINT
  {
    $$ = new IntExp(p, -$2);
  }
  ;

CONSTLIST: // empty
  {
    $$ = new vector<IntExp*>();
  }
  |
  CONST CONSTREST
  {
    vector<IntExp*> *v = $2;
    v->push_back($1);
    rotate(v->begin(), v->end() - 1, v->end());
    $$ = v;
  }
  ;

CONSTREST: // empty
  {
    $$ = new vector<IntExp*>();
  }
  |
  ',' CONST CONSTREST
  {
    vector<IntExp*> *v = $3;
    v->push_back($2);
    rotate(v->begin(), v->end() - 1, v->end());
    $$ = v;
  }
  ;


STMLIST: // empty
  {
#ifdef DEBUG
    cerr << "STMLIST empty" << endl;
#endif
    $$ = new vector<Stm*>();
  }
  |
  STM STMLIST
  {
#ifdef DEBUG
    cerr << "STM STMLIST" << endl;
#endif
    vector<Stm*> *v = $2;
    v->push_back($1);
    rotate(v->begin(), v->end() - 1, v->end());
    $$ = v;
  }
  ;

STM: '{' STMLIST '}'
  {
    $$ = new Nested(p, $2);
  }
  |
  IF '(' EXP ')' STM %prec THEN
  {
    $$ = new If(p, $3, $5);
  }
  |
  IF '(' EXP ')' STM ELSE STM
  {
    $$ = new If(p, $3, $5, $7);
  }
  |
  WHILE '(' EXP ')' STM
  {
    $$ = new While(p, $3, $5);
  }
  |
  WHILE '(' EXP ')' ';'
  {
    $$ = new While(p, $3);
  }
  |
  EXP '=' EXP ';'
  {
    $$ = new Assign(p, $1, $3);
  }
  |
  EXP '.' ID '(' EXPLIST ')' ';'
  {
    $$ = new CallStm(p, $1, $3, $5);
  }
  |
  CONTINUE ';'
  {
    $$ = new Continue(p);
  }
  |
  BREAK ';'
  {
    $$ = new Break(p);
  }
  |
  RETURN EXP ';'
  {
    $$ = new Return(p, $2);
  }
  |
  PUTINT '(' EXP ')' ';'
  {
    $$ = new PutInt(p, $3);
  }
  |
  PUTCH '(' EXP ')' ';'
  {
    $$ = new PutCh(p, $3);
  }
  |
  PUTARRAY '(' EXP ',' EXP ')' ';'
  {
    $$ = new PutArray(p, $3, $5);
  }
  |
  STARTTIME '(' ')' ';'
  {
    $$ = new Starttime(p);
  }
  |
  STOPTIME '(' ')' ';'
  {
    $$ = new Stoptime(p);
  }
  ;

EXP: NONNEGATIVEINT
  {
    $$ = new IntExp(p, $1);
  }
  |
  TRUE
  {
    $$ = new BoolExp(p, true);
  }
  |
  FALSE
  {
    $$ = new BoolExp(p, false);
  }
  |
  LENGTH '(' EXP ')'
  {
    $$ = new Length(p, $3);
  }
  |
  GETINT '(' ')'
  {
    $$ = new GetInt(p);
  }
  |
  GETCH '(' ')'
  {
    $$ = new GetCh(p);
  }
  |
  GETARRAY '(' EXP ')'
  {
    $$ = new GetArray(p, $3);
  }
  |
  ID
  {
    $$ = $1;
  }
  |
  THIS
  {
    $$ = new This(p);
  }
  |
  EXP ADD EXP
  {
#ifdef DEBUG
    cerr << "EXP ADD EXP" << endl;
#endif
    Pos *p1 = new Pos(@2.sline, @2.scolumn, @2.eline, @2.ecolumn);
    $$ = new BinaryOp(p, $1, new OpExp(p1, "+"), $3);
  }
  |
  EXP MINUS EXP
  {
#ifdef DEBUG
    cerr << "EXP MINUS EXP" << endl;
#endif
    Pos *p1 = new Pos(@2.sline, @2.scolumn, @2.eline, @2.ecolumn);
    $$ = new BinaryOp(p, $1, new OpExp(p1, "-"), $3);
  }
  |
  EXP TIMES EXP
  {
#ifdef DEBUG
    cerr << "EXP TIMES EXP" << endl;
#endif
    Pos *p1 = new Pos(@2.sline, @2.scolumn, @2.eline, @2.ecolumn);
    $$ = new BinaryOp(p, $1, new OpExp(p1, "*"), $3);
  }
  |
  EXP DIVIDE EXP
  {
#ifdef DEBUG
    cerr << "EXP DIVIDE EXP" << endl;
#endif
    Pos *p1 = new Pos(@2.sline, @2.scolumn, @2.eline, @2.ecolumn);
    $$ = new BinaryOp(p, $1, new OpExp(p1, "/"), $3);
  }
  |
  EXP EQ EXP
  {
#ifdef DEBUG
    cerr << "EXP EQ EXP" << endl;
#endif
    Pos *p1 = new Pos(@2.sline, @2.scolumn, @2.eline, @2.ecolumn);
    $$ = new BinaryOp(p, $1, new OpExp(p1, "=="), $3);
  }
  |
  EXP NE EXP
  {
#ifdef DEBUG
    cerr << "EXP NE EXP" << endl;
#endif
    Pos *p1 = new Pos(@2.sline, @2.scolumn, @2.eline, @2.ecolumn);
    $$ = new BinaryOp(p, $1, new OpExp(p1, "!="), $3);
  }
  |
  EXP LT EXP
  {
#ifdef DEBUG
    cerr << "EXP LT EXP" << endl;
#endif
    Pos *p1 = new Pos(@2.sline, @2.scolumn, @2.eline, @2.ecolumn);
    $$ = new BinaryOp(p, $1, new OpExp(p1, "<"), $3);
  }
  |
  EXP LE EXP
  {
#ifdef DEBUG
    cerr << "EXP LE EXP" << endl;
#endif
    Pos *p1 = new Pos(@2.sline, @2.scolumn, @2.eline, @2.ecolumn);
    $$ = new BinaryOp(p, $1, new OpExp(p1, "<="), $3);
  }
  |
  EXP GT EXP
  {
#ifdef DEBUG
    cerr << "EXP GT EXP" << endl;
#endif
    Pos *p1 = new Pos(@2.sline, @2.scolumn, @2.eline, @2.ecolumn);
    $$ = new BinaryOp(p, $1, new OpExp(p1, ">"), $3);
  }
  |
  EXP GE EXP
  {
#ifdef DEBUG
    cerr << "EXP GE EXP" << endl;
#endif
    Pos *p1 = new Pos(@2.sline, @2.scolumn, @2.eline, @2.ecolumn);
    $$ = new BinaryOp(p, $1, new OpExp(p1, ">="), $3);
  }
  |
  EXP AND EXP
  {
#ifdef DEBUG
    cerr << "EXP AND EXP" << endl;
#endif
    Pos *p1 = new Pos(@2.sline, @2.scolumn, @2.eline, @2.ecolumn);
    $$ = new BinaryOp(p, $1, new OpExp(p1, "&&"), $3);
  }
  |
  EXP OR EXP
  {
#ifdef DEBUG
    cerr << "EXP OR EXP" << endl;
#endif
    Pos *p1 = new Pos(@2.sline, @2.scolumn, @2.eline, @2.ecolumn);
    $$ = new BinaryOp(p, $1, new OpExp(p1, "||"), $3);
  }
  |
  NOT EXP
  {
#ifdef DEBUG
    cerr << "! EXP" << endl;
#endif
    Pos *p1 = new Pos(@1.sline, @1.scolumn, @1.eline, @1.ecolumn);
    $$ =  new UnaryOp(p, new OpExp(p1, "!"), $2);
  }
  |
  MINUS EXP %prec UMINUS
  {
#ifdef DEBUG
    cerr << "- EXP" << endl;
#endif
    Pos *p1 = new Pos(@1.sline, @1.scolumn, @1.eline, @1.ecolumn);
    $$ =  new UnaryOp(p, new OpExp(p1, "-"), $2);
  }
  |
  '(' EXP ')'
  {
#ifdef DEBUG
    cerr << "( EXP )" << endl;
#endif
    $$ = $2;
  }
  |
  '(' '{' STMLIST '}' EXP ')'
  {
#ifdef DEBUG
    cerr << "( { STMLIST } EXP )" << endl;
#endif
    $$ = new Esc(p, $3, $5);
  }
  |
  EXP '.' ID
  {
    $$ = new ClassVar(p, $1, $3);
  }
  |
  EXP '.' ID '(' EXPLIST ')' %prec METHOD_CALL
  {
    $$ = new CallExp(p, $1, $3, $5);
  }
  |
  EXP '[' EXP ']' %prec ARRAY_ACCESS
  {
    $$ = new ArrayExp(p, $1, $3);
  }
  ;

EXPLIST: // empty
  {
    $$ = new vector<Exp*>();
  }
  |
  EXP EXPREST
  {
    vector<Exp*> *v = $2;
    v->push_back($1);
    rotate(v->begin(), v->end() - 1, v->end());
    $$ = v;
  }
  ;

EXPREST: // empty
  {
    $$ = new vector<Exp*>();
  }
  |
  ',' EXP EXPREST
  {
    vector<Exp*> *v = $3;
    v->push_back($2);
    rotate(v->begin(), v->end() - 1, v->end());
    $$ = v;
  }
  ;

CLASSDECLLIST: // empty
  {
#ifdef DEBUG
    cerr << "CLASSDECLLIST empty" << endl;
#endif
    $$ = new vector<ClassDecl*>();
  }
  |
  CLASSDECL CLASSDECLLIST
  {
    vector<ClassDecl*> *v = $2;
    v->push_back($1);
    rotate(v->begin(), v->end() - 1, v->end());
    $$ = v;
  }
  ;

CLASSDECL: PUBLIC CLASS ID '{' VARDECLLIST METHODDECLLIST '}'
  {
    $$ = new ClassDecl(p, $3, $5, $6);
  }
  |
  PUBLIC CLASS ID EXTENDS ID '{' VARDECLLIST METHODDECLLIST '}'
  {
    $$ = new ClassDecl(p, $3, $5, $7, $8);
  }
  ;

METHODDECLLIST: // empty
  {
    $$ = new vector<MethodDecl*>();
  }
  |
  METHODDECL METHODDECLLIST
  {
    vector<MethodDecl*> *v = $2;
    v->push_back($1);
    rotate(v->begin(), v->end() - 1, v->end());
    $$ = v;
  }
  ;

METHODDECL: PUBLIC TYPE ID '(' FORMALLIST ')' '{' VARDECLLIST STMLIST '}'
  {
    $$ = new MethodDecl(p, $2, $3, $5, $8, $9);
  }
  ;

TYPE: CLASS ID
  {
    $$ = new Type(p, $2);
  }
  |
  INT
  {
    $$ = new Type(p);
  }
  |
  INT '[' ']'
  {
    $$ = new Type(p, new IntExp(p, 0));  // ???
  }
  ;

FORMALLIST: // empty
  {
    $$ = new vector<Formal*>();
  }
  |
  TYPE ID FORMALREST
  {
    vector<Formal*> *v = $3;
    v->push_back(new Formal(p, $1, $2));
    rotate(v->begin(), v->end() - 1, v->end());
    $$ = v;
  }
  ;

FORMALREST: // empty
  {
    $$ = new vector<Formal*>();
  }
  |
  ',' TYPE ID FORMALREST
  {
    vector<Formal*> *v = $4;
    v->push_back(new Formal(p, $2, $3));
    rotate(v->begin(), v->end() - 1, v->end());
    $$ = v;
  }
  ;

ID: IDENTIFIER
  {
    $$ = new IdExp(p, $1);
  }
  ;

%%
/*
void yyerror(char *s) {
  fprintf(stderr, "%s\n",s);
}

int yywrap() {
  return(1);
}
*/

//%code 
namespace fdmj 
{
    template<typename RHS>
    inline void calcLocation(location_t &current, const RHS &rhs, const std::size_t n)
    {
        current = location_t(YYRHSLOC(rhs, 1).sline, YYRHSLOC(rhs, 1).scolumn, 
                                    YYRHSLOC(rhs, n).eline, YYRHSLOC(rhs, n).ecolumn);
        p = new Pos(current.sline, current.scolumn, current.eline, current.ecolumn);
    }
    
    void ASTParser::error(const location_t &location, const std::string &message)
    {
        std::cerr << "Error at lines " << location << ": " << message << std::endl;
    }

  Program* fdmjParser(ifstream &fp, const bool debug) {
    fdmj:AST_YYSTYPE result; 
    result.root = nullptr;
    fdmj::ASTLexer lexer(fp, debug);
    fdmj::ASTParser parser(lexer, debug, &result); //set up the parser
    if (parser() ) { //call the parser
      cout << "Error: parsing failed" << endl;
      return nullptr;
    }
    if (debug) cout << "Parsing successful" << endl;
    return result.root;
  }

  Program*  fdmjParser(const string &filename, const bool debug) {
    std::ifstream fp(filename);
    if (!fp) {
      cout << "Error: cannot open file " << filename << endl;
      return nullptr;
    }
    return fdmjParser(fp, debug);
  }
}
