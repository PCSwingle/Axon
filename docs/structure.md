### Axon Structure

Code is contained in `.ax` files. Put a `axon.toml` file at the root of your project. It contains build options; the
only required options are `main="<path>"`, which points to the main file of the project, and `name="<name>"`, which will
set the name of the module. Run the compiler in the directory containing the build file or point to it directly.

### Dependencies

Specified in `axon.toml`. TODO: need some way to avoid name conflicts

#### Compilation

Only files that are directly imported are included.

TODO: should external modules be bundled into the compilation unit? this way you wouldn't need to include i.e. entire
stdlib. Middle ground could be: external modules are compiled in separate compilation unit, but only the parts you
actually use (if this is even possible or good?).

#### Importing

To export a function or struct from a file, simply make it `public`. Exported objects will be under the namespace
`module.dir.file.object` starting from the dir of `axon.toml`. This can be imported with the following statement:
`from module.dir.file import object, object2 as obj2`. For example, in a project with the following structure:

```
.
├── axon.toml
├── main.ax
└── dir
    └── test.ax

### axon.toml ###
name = "my_module"
main = "my_module.main"

### dir/test.ax ###
public func add(a: int, b: int): int {
    return a + b
}

### main.ax ###
# We can import the function normally...
from my_module.dir.test import add

# ...or use an alias:
from my_module.dir.test import add as add_func

func main() {
    add(2, 2)
    add_func(2, 2)
}
```