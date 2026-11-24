# Virgil Grammar

Extracted from `aeneas/src/vst/Parser.v3` (branch `annotations-v2`). The notation is EBNF:

```
X?   optional        X*   zero or more        X+   one or more
X,*  comma-separated list, zero or more       X,+  same, at least one
'tok' literal token   id  identifier          [flag]  production enabled only by -lang:<flag>
```

A `,*` / `,+` list inside a delimiter (`()`, `[]`, `<>`) may have one trailing comma before its
closing delimiter (e.g. `[1, 2, 3,]`, `f(a, b,)`, `List<int,>`). This does not apply to declarator
lists such as `var x = 1, y = 2;`, which end in `;`.

Feature flags (`aeneas/src/main/CLOptions.v3`):

| Flag | Default | Enables |
|---|---|---|
| `-lang:fun-exprs` | on | `fun` expressions |
| `-lang:simple-bodies` | on | `=> Expr` bodies |
| `-lang:open-types` | off (UNSTABLE builds only) | `case _`, variant `extends` |
| `-lang:descriptors` | off (UNSTABLE only) | class `descriptor` / `describes` clauses |
| `-lang:annotations` | off (UNSTABLE only) | all `@...` forms. `Anns` below means `Annotation*` when the flag is on and nothing when it is off |

---

## Top Level

```
File ::= FileAnnotation* ToplevelDecl*

FileAnnotation ::= [annotations] '@file' '[' AnnArg,+ ']'

ToplevelDecl ::= Anns
    ( 'private'? 'class'              ClassDecl
    | 'private'? 'component'          id Members
    | 'private'? 'thread' 'component' id Members
    | 'private'? 'type'               VariantDecl
    | 'private'? 'enum'               EnumDecl
    | 'private'? 'packing'            PackingDecl
    | 'private'? '@type'              AnnotationTypeDecl     // [annotations]
    | 'private'? '@def'               AnnotationAlias        // [annotations]
    | 'import' string? 'component'    id Members             // members may carry import names
    | 'layout'                        LayoutDecl
    | 'export'                        ExportDecl             // annotations are an error here
    | 'var'                           FieldDecls             // members of the file's synthetic component
    | 'def'                           DefDecl )
```

---

## Declarations

```
ClassDecl ::= id TypeParams? ClassParams?
              ('extends' TypeRef)?
              TupleExpr?                          // super-constructor args
              DescriptorInfo?                     // [descriptors]
              RepHints Members
DescriptorInfo ::= ('descriptor' TypeRef)? ('describes' TypeRef)?

VariantDecl   ::= id TypeParams? ClassParams?
                  ('extends' TypeRef)?            // [open-types]
                  RepHints (';' | '{' VariantMember* '}')
VariantMember ::= Anns
                  ( 'def' DefDecl                 // no 'private'; may be a method, index method, or field
                  | 'case' id CaseParams? RepHints (';' | Members)
                  | 'case' '_' RepHints (';' | Members) )   // [open-types] default case

EnumDecl ::= id EnumParams? '{' EnumCase* '}'
EnumCase ::= Anns id ('(' Expr,* ')')? ','?

PackingDecl  ::= id '(' PackingParam,* ')' ':' IntLiteral '=' PackingExpr ';'
PackingParam ::= Anns id ':' IntLiteral
PackingExpr  ::= BitPattern                              // 0b... containing '?' or letters
               | '#' 'solve'  '(' PackingExpr,* ')'      // not in packing decls, not nested
               | '#' 'concat' '(' PackingExpr,* ')'
               | id TypeArgs? ('(' PackingExpr,* ')')?   // application or field
               | Number
BitPattern   ::= '0' ('b'|'B') ('0' | '1' | '?' | Letter | '_')*

LayoutDecl  ::= id RepHints '{' LayoutField* '=' IntLiteral ';' '}'
LayoutField ::= '+' IntLiteral Anns id ':' MemoryTypeRef RepHints ';'

ExportDecl    ::= string? 'def' DefDecl                  // an explicit name applies to exactly one decl
                | id TypeArgs? ('=' DottedVarExpr)? ';'
                | string '=' DottedVarExpr ';'
DottedVarExpr ::= id TypeArgs? ('.' (id TypeArgs? | AnnotationName))*
```

### Annotation declarations `[annotations]`

```
AnnotationTypeDecl ::= id ('[' AnnParam,* ']')? (';' | Members)    // only 'def' members allowed
AnnotationAlias    ::= id '=' Annotation (',' Annotation)* ';'
AnnParam           ::= Anns id ':' TypeRef ('=' Expr)?
```

### Variant constraints (checked by the parser or verifier)

- `case` is an error if the variant head has parameters (`type T(x: int) { case A; }`).
- `case _` may appear at most once, and only after every named case. Its `Members` may not contain
  `new` or `var`.

---

## Members

```
Members ::= '{' Member* '}'
Member  ::= Anns 'private'? string?                // the string (import name) only inside 'import component'
            ( 'def' 'var'? DefDecl                 // 'private def var' is an error
            | 'new' NewDecl
            | 'var' FieldDecls )

NewDecl ::= MethodParams (':' 'super' TupleExpr | 'super' TupleExpr)? BlockStmt

DefDecl ::= IndexDecl
          | id TypeParams? TypedParams ReturnTypeAndBody     // method (TypeParams always make it a method)
          | FieldDecls                                        // read-only field(s)

IndexDecl ::= '[' TypedParam,* ']'
              ( '=' TypedParam (';' | BlockStmt)             // setter, named "[]="
              | ReturnTypeAndBody )                          // getter, named "[]"

FieldDecls ::= FieldDecl (',' FieldDecl)* ';'
FieldDecl  ::= id (':' TypeRef)? ('=' Expr)? RepHints        // hints AFTER the initializer

ReturnTypeAndBody ::=
    '->' ('this' | TypeRef) RepHints
         ( '=>' Expr ';'                    // [simple-bodies]  (no ';' in a fun expression)
         | ';'
         | BlockStmt )
  | '=>' Expr ';'                           // [simple-bodies]  implicit return type
  | RepHints (';' | BlockStmt)
```

---

## Parameters and Types

```
ClassParams  ::= '(' ClassParam,* ')'      // class/variant head: 'var' allowed, type required, default def
EnumParams   ::= '(' EnumParam,* ')'       // no 'var', type required
CaseParams   ::= EnumParams                // variant case params
TypedParams  ::= '(' TypedParam,* ')'      // def methods, nested local defs: 'var' allowed, type required
MethodParams ::= '(' MethodParam,* ')'     // new, fun: 'var' allowed, type optional

ClassParam  ::= Anns 'var'? id ':' TypeRef
EnumParam   ::= Anns id ':' TypeRef
TypedParam  ::= Anns 'var'? id ':' TypeRef
MethodParam ::= Anns 'var'? id (':' TypeRef)?

TypeParams ::= '<' TypeParam,+ '>'
TypeParam  ::= Anns id
TypeArgs   ::= '<' TypeRef,+ '>'

TypeRef ::= Anns
            ( '(' TypeRef,* ')'                  // tuple type; () is void
            | id TypeArgs? ('.' id TypeArgs?)* ) // (qualified) named type
            ('->' TypeRef)*                      // function type

MemoryTypeRef ::= Anns id ('[' IntLiteral ']')?
```

---

## Statements

```
Stmt ::= AnnotatedStmt
       | BlockStmt
       | ';'
       | 'if'    '(' Expr ')' Stmt ('else' Stmt)?
       | 'while' '(' Expr ')' Stmt
       | 'for'   '(' LoopVar '<'  Expr ')' Stmt                // range:   for (i < n)
       | 'for'   '(' LoopVar 'in' Expr ')' Stmt                // foreach: for (x in xs)
       | 'for'   '(' LoopVar ';' Expr ';' Expr? ')' Stmt       // C-style; update may be empty
       | 'match' '(' Expr ')' '{' MatchCase* '}' ('else' Stmt)?
       | 'var' LocalVars
       | 'def' id TypedParams ReturnTypeAndBody                 // nested function (no type params)
       | 'def' LocalVars                                        // read-only locals
       | 'break' ';'
       | 'continue' ';'
       | 'return' Expr? ';'
       | Expr ';'

AnnotatedStmt ::= [annotations] Annotation+ (BlockStmt | 'var' LocalVars | 'def' ...)
                  // annotations on any other statement kind are an error
BlockStmt ::= '{' Stmt* '}'
LocalVars ::= LocalVar (',' LocalVar)* ';'
LocalVar  ::= id (':' TypeRef)? RepHints ('=' Expr)?    // hints BEFORE the initializer
LoopVar   ::= id (':' TypeRef)? ('=' Expr)?             // no hints
```

---

## Match Cases

```
MatchCase    ::= MatchPattern (',' MatchPattern)* '=>' Stmt

MatchPattern ::= '_'                                  // default (also allowed inside a comma list)
               | 'true' | 'false' | 'null'
               | id ':' '_'                           // named default: binds matched value to id
               | id ':' TypeRef                       // type-test binding
               | DottedVarExpr ('(' MatchParam,* ')')?   // case / constant, optional destructuring
               | ByteLiteral
               | Number                               // a leading '-' is allowed

MatchParam ::= '_' | id
```

---

## Expressions

The parser handles binary operators with an operator-precedence stack (`addBinOpSuffixes`), using
the precedences in `V3Infix`, instead of nested grammar rules. Compound assignments use the same
stack at precedence 0.

```
Expr    ::= SubExpr '=' Expr                 // assignment, right-associative ('=' not '==')
          | SubExpr (BinOp SubExpr)*

SubExpr ::= Term Suffix*

Suffix  ::= '.' MemberRef
          | '(' Expr,* ')'                   // call
          | '[' ']'                          // empty index
          | '[' Expr '...' Expr? ']'         // range slice [a ... b]
          | '[' Expr '..+' Expr ']'          // range slice [a ..+ len]
          | '[' Expr (',' Expr)* ']'         // index / multi-index
          | '++' | '--'                      // postfix; ends the suffix chain

MemberRef ::= id TypeArgs?                   // named member; keywords are accepted here
            | ('!' | '?') TypeArgs?          // cast / query, e.g. x.!<int>
            | BinOp                          // operator member, e.g. .+  .==
            | decimal                        // tuple element, e.g. .0
            | AnnotationName                 // [annotations] annotation query, e.g. x.@ann
            | '~'
            | '[]=' | '[]'                   // index operator members

Term ::= [annotations] Annotation+ '(' ...   // annotated parenthesized expression
       | 'if' '(' Expr,+ ')'                 // if-expression; the verifier checks for exactly 3 args
       | 'fun' FunHead ReturnTypeAndBody     // [fun-exprs]
       | 'true' | 'false' | 'null'
       | id TypeArgs?                        // variable reference ('this' is an ordinary identifier)
       | Number
       | [annotations] AnnotationName Suffix* (BinOp SubExpr)*   // annotation used as a value
       | ByteLiteral
       | string
       | '(' Expr,* ')'                      // tuple / parenthesized
       | '[' Expr,* ']'                      // array literal
       | '!' SubExpr                         // logical not
       | '~' SubExpr                         // bitwise complement
       | '-' SubExpr                         // negation ('-' followed by a digit is a negative literal)
       | '--' SubExpr | '++' SubExpr         // prefix decrement / increment
       | '_'                                 // partial application hole

FunHead ::= id MethodParams                  // named function expression
          | id ':' TypeRef                   // single typed param (currently broken; see Known Parser Issues)
          | id                               // single untyped param
          | MethodParams
          | /* empty */

BinOp ::=
    '+' | '-' | '*' | '/' | '%'                         // arithmetic
  | '&' | '|' | '^' | '<<' | '>>' | '>>>'               // bitwise ('>>' arithmetic, '>>>' logical)
  | '==' | '!=' | '<' | '<=' | '>' | '>='               // comparison
  | '&&' | '||'                                         // logical
  | '+=' | '-=' | '*=' | '/=' | '%='                    // compound assignment
  | '&=' | '|=' | '^=' | '<<=' | '>>=' | '>>>='
```

---

## Representation Hints

```
RepHints ::= RepHint*

RepHint ::= '#' 'packing' ('(' PackingExpr,* ')' | PackingExpr)
          | '#' HintName                       // boxed, unboxed, packed, big-endian, inline, no-inline
          | '#' HintName '(' Expr,* ')'        // expression hint (other names only)
          | '#' HintName '<' TypeRef,* '>'     // type hint (other names only)
          | '#' HintName                       // any other name

HintName ::= IdentStart (IdentMiddle | '-' | ':')*     // a keyword other than 'packing' is an error
```

Hints can be attached to classes, variants, variant cases (including `case _`), layouts, layout
fields, fields, `var`/`def` locals, and method return types (after `-> T`, or before the body of a
method with no return type). Enums and components cannot have hints.

---

## Annotations `[annotations]`

```
Annotation     ::= AnnotationName ('[' AnnArg,* ']')?   // not followed by '.'
AnnotationName ::= '@' IdentStart IdentMiddle*          // must not be a keyword
AnnArg         ::= Annotation* (id ':')? AnnValue?      // with no value and no 'id:', the last
                                                        // annotation is the value
AnnValue       ::= AnnotationName Suffix* (BinOp SubExpr)* | Expr
```

`@name` followed by `.` is an annotation *reference* (an expression), not an annotation.

---

## Lexical

```
id          ::= IdentStart IdentMiddle*
IdentStart  ::= [a-zA-Z]                  // '_' is NOT an ident-start; alone it is a separate token
IdentMiddle ::= [a-zA-Z0-9_]

Number      ::= DecLiteral | HexLiteral | BinLiteral | FloatLiteral   // lib/util/NumberParser.v3
              // 0x hex, 0b binary, fraction/exponent floats, '_' digit separators
              // int suffixes: u/U, l/L, explicit widths uN / iN (e.g. u8, i64)
              // float suffixes: f/F, d/D
IntLiteral  ::= Number that is an integer
string      ::= '"' (printable | escape)* '"'
ByteLiteral ::= '\'' (printable | escape) '\''

Whitespace  ::= ' ' | '\t' | '\n'
Comment     ::= '//' ... end-of-line | '/*' ... '*/'      // block comments do not nest
              // any other byte outside ' '..'~' (including '\r') is an "invalid character" error

Keywords    ::= break case class component continue def descriptor describes
                else enum export extends false for fun if import in layout
                match new null packing private return struct super thread
                true type var while
              // 'this' is not a keyword
```

---

## Known Parser Issues

- **`fun x: T` does not parse.** In `parseTerm` (`aeneas/src/vst/Parser.v3`, around line 1197), the
  single-typed-parameter branch of a `fun` expression calls `parseTypeRef(p)` without first consuming
  the `:`. As a result `def f = fun x: int => x + 1;` fails with `ParseError: identifier expected` at
  the `:`. The workaround is the parenthesized form `fun (x: int) => x + 1`. The likely fix is to call
  `p.advance1()` before `parseTypeRef`. No test covers this form. (Found 2026-11-23; not yet fixed.)
