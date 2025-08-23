```
<Delimiter> -> ; | \n
<Type> -> int | long | <Identifier> | ...
<Identifier> -> [a-zA-Z][a-zA-Z0-9_]*
<BinaryOp> -> + | - | ...
<UnaryOp> -> - | ! | ... # worth pointing out `-` can be both unary and binary
<VarOp> -> = | += | -= | ...

<Call> -> ([<Expr>,]*)
<Constructor> -> ~<Identifier> { [<Identifier>: <Expr>,]* }
<Array> -> ~[[<Expr>,]*]
<Expr> -> <Identifier> | <Value> | <Expr><Call> | <Constructor> | <Array> | <Expr> <BinaryOp> <Expr> | <UnaryOp> <Expr> | ( <Expr> )

<If> -> if (<Expr>) <Block> [elif (<Expr>) <Block>]* [else <Block>]?
<While> -> while (<Expr>) <Block>

<Var> -> <Type>? <Identifier> <VarOp> <Expr>
<Func> -> extern? func <Identifier>([<Type> <Identifier>,]*): <Type> <Block>?

<Struct> -> struct <Identifier> { [<Type> <Identifier> <Delimiter>]*

<Statement> -> [<Var> | <Expr> | <Func> | <If> | <While> | <Struct> | return <Expr> | ...] <Delimiter> (should blocks count as statements?)
<Block> -> { <Statement>* }

<TopLevel> -> <Block> (but without {})
```