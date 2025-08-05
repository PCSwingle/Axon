### Axon Structure

Code is contained in `.ax` files. To compile one file, pass the file to the compiler. To compile an entire directory,
pass the directory to the compiler and it will compile all Axon files within the directory.

#### Compilation

TODO: should external libraries / dependencies be bundled into the compilation unit? this way you wouldn't need to
include i.e. entire stdlib. Middle ground could be: external dependencies are compiled in separate compilation unit, but
_only the parts you actually use_.

#### Importing

To export a function or struct from a file, simply make it `public`. Exported objects will be under the namespace
`dir.file.object`. This can be imported as `import dir`, `import dir.file`, or `import dir.file.object` to be used as
`dir`, `file`, or `object` respectively, or as an alias using `as alias`. For example:

```
src
├── main.ax
└── dir
    └── test.ax

### dir/test.ax ###
public func add(a: int, b: int): int {
    return a + b
}

### main.ax ###
# We can import the entire directory...
import dir

# ...or the entire file...
import dir.test

# ...or just the function we want.
import dir.test.add

# We can also alias the function:
import dir.test as add_file

func main() {
    # It can now be called in these four respective ways:
    dir.test.add(2, 2)
    test.add(2, 2)
    add(2, 2)
    add_file.add(2, 2)
}
```