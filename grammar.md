```
<Type> -> int | long | ...
<Identifier> -> [a-zA-Z][a-zA-Z0-9_]*
<BinaryOp> -> + | - | ...
<UnaryOp> -> - | ! | ... # worth pointing out `-` can be both unary and binary

<Call> -> <Identifier>([<Expr>,]*)
<Expr> -> <Identifier> | <Value> | <Call> | <Expr> <BinaryOp> <Expr> | <UnaryOp> <Expr> | ( <Expr> )

<If> -> if (<Expr>) <Block> [elif (<Expr>) <Block>]* [else <Block>]?
<While> -> while (<Expr>) <Block>

<Var> -> <Type>? <Identifier> [= | += | -= | ...] <Expr>
<Func> -> native? func <Type> <Identifier>([<Type> <Identifier>,]*) <Block>?

<Statement> -> <Var> | <Expr> | <Func> | <If> | <While> | return <Expr> ... (should blocks count as statements?)
<Block> -> { <Statement>* }

<TopLevel> -> <Block> (but without {})
```