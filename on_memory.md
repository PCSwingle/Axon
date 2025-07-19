### The Plan

* No manual memory management (i.e. you never need to call malloc or free)
* Memory access after free _impossible_
* Memory leaks only possible with scenarios like circular references
    * Importantly, only possible if you explicitly use refcounting

### The Implementation

* Everything is a pointer to an object on the heap other than primitives
* Pointers don't exist for user
* null does not exist, but any type can be optional with a `?` after the type (`Object?`)
    * Can forcibly use an optional with `!`, but will throw an exception if it is null (so still memory safe).
* Objects automaticaly malloc on creation and free on deletion
    * You can override destructor too.
* Arrays will also be an object, but obviously special (since only way to get n contiguous memory space)
    * Arrays always check on indexing!
* Objects will be one of 3 types:
    * Owner. This is what you get when you `~` (equivalent of new) an object.
        * Owners have to have ~ attached to their type (`Object~ obj = ~Object()`)
        * When owner goes out of scope, memory is freed.
        * All owned sub objects are also freed.
        * Only one owner ever; if a function takes an owner (`func foo(Object~ obj)`), you no longer own it.
        * In call you have to mark it like so: (`foo(~obj)`)
    * Reference. This is the default for variables passed in.
        * Owners will automatically be converted to references when passed in.
        * References cannot be upgraded to owners.
        * Lifetime tracking similar to rusts; whenever a reference is controlled by an object, that object _must_ go out
          of scope before the owner of that reference does.
            * On compile time: track reference scopes (equivalent of lifetimes).
                * Special scope: global scope (equivalent of `'static`). These objects are denoted with `global`.
                    * Note: you can create global objects anywhere; although obviously you should be aware that it won't
                      be cleaned up until program end! It's a global object!
                * When you allocate a regular object in a scope, you expect all of it's references to be cleaned up by
                  end of scope.
                * So objects that were alive before this scope cannot control references to objects created in this
                  scope.
                    * Seems overly restrictive? it is! but if you take ownership of an object when you enter the scope,
                      you know it will be cleaned up at end of scope.
                    * Alternatively, you can move the object to the object you want to own it.
                * Owners can only move up scope manually by being returned from a function.
    * Refcounted (rare / manual). Manually mark a variable as refcounted with `Object# obj = #Object()`.
        * Only way to cause memory leaks (through circular ref counted loops like linked lists).
        * Act like owners, but unlike owners the original refcounted variable is still available after passing in.
        * Can still create weak refs like normal, and these will have the same restrictions as regular refs.
        * Whenever goes out of scope, auto checks if count is 0 and if so frees just like owners.

### Possible other features

* Possible rust unsafe equivalent for pointers, malloc, free, unsafe ownership / reference control, and access arrays
  without checking bounds?