# Axon

## Language Basics

### Comments

```
# This is a comment
/*
This is a multi-line comment
*/
```

### Types

```
let my_bool: bool = True | False
let my_32bit_num = 0
let my_64bit_num: long = 0L
let optional_int: int? = null # all types can be optional, i.e. can be null
x = optional_int! + n # at your own risk you can assert that it isn't null
```

### Variables

```
let x = 2 # Semicolons not required (but are allowed)
let b = ( a ||
           b ||
           c
         ) # Multi-line expressions require parentheses
if (b) {
    # Shadowing not allowed
    let b: bool = False # Will crash!
}
```

### Operators

```
= # Assignment
+ # Addition
- # Subtraction
* # Multiplication
/ # Division
% # Modulus
< # Less than
> # Greater than
! # Boolean not
== # Eq
!= # Neq
<= # Lte
>= # Gte
|| # Boolean or
&& # Boolean and
<< # Left shift
>> # Right shift
^ # Bitwise xor
& # Bitwise and
| # Bitwise or
~ # Bitwise not
```

### Functions

```
func my_func(int var1, int var2): int {
    return var1 + var2
}
```

### Control

```
if (x) {
    # Do work...
} elif (y) {
    # Do other work...
} else {
    # You get the idea
}

while (b) {
    # Do things...
}

# TODO: determine for loop syntax
for () {

}
```

### Exceptions

```
TODO; but basic idea: all functions have to signal if they raise errors (except possibly one type of error so users have the option to be lazy).
Problem; do I really want users to have to handle it every single time they access an array, perform division, or assert non-nullableness?
```

### Coming soon

Classes, OOP, strings, arrays, exceptions, imports / linking, etc.