defer to object destruction time

varargs

named args

string interns! the perfect solution:

* Doing `~"my string"` will _construct_ the string and hence give you _ownership_ of the string.
* Doing just `"my string"` (which is the default, and what most people will expect!) just gives you an _immutable
  reference_ to the string, which means it makes _inutitive sense_ that it it's interned!
* Sidenote: Allowing bare arrays with a `~` could make this the first language with interned arrays; is this a good
  decision though?
    * Realistically, there are very few use cases for an interned array, and it's much much more likely that people get
      tripped up by it
    * On the other hand, it should be clear that the variable type is an immutable reference and therefore maybe it
      won't be confusing?
    * I mean worst case somebody accidentally interns an array and has to put one character in to fix it when they want
      to change the array.
    * Something worth noting is that interned arrays will have to own all of their values (and this makes intuitive
      sense!)

loicense!