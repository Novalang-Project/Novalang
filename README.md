# NovaLang

**NovaLang** is a lightweight, gradually typed programming language designed for **simplicity, rapid prototyping, and experimentation**. It combines the flexibility of dynamic languages with optional type annotations for safer and more structured code.

NovaLang uses a **bytecode interpreter architecture**: source code is parsed into an Abstract Syntax Tree (AST), compiled into bytecode, and then executed by a virtual machine. This design allows for faster execution than direct AST interpretation while remaining simple and portable.

The language aims to be approachable for beginners while still powerful enough for scripting, automation, and experimental systems and AI-assisted workflows (planned).

---

# Features

## Core Language

NovaLang currently supports the following language features:

### Variables and Types

* Built-in primitive types:

  * `int`
  * `float`
  * `string`
  * `bool`
* Type inference using `auto`
* Any variable type using `any`, meaning variable types can change during runtime.
* Optional type annotations for stricter code

Example:

```cpp
auto x = 10
string name = "Nova"

any y = 20
y = "Lang"
```

---

### Arithmetic and Expressions

NovaLang supports standard mathematical operations:

```
+   addition
-   subtraction
*   multiplication
/   division
^   exponentiation
```

Expressions can be grouped using parentheses.

```py
int result = (a + b) * 2
```

---

### Unary Operations

```
++   increment
--   decrement
-    negation
```

Example:

```c
x++
y = -x
```

---

### Functions

Functions support parameters, return values, and local scoping.

Example:

```c
func add(a: int, b: int) -> int
{
    return a + b
}
```

NovaLang allows **optional type annotations**, enabling gradual typing.

---

### Structs

Simple data structures can be defined and accessed using dot notation.

Example:

```cpp
struct User {
    string name = ""
}
 
User currentUser = User(name="Alice")
println("Current user:", currentUser.name)
```

---

### Control Flow

NovaLang includes common control structures:

```c
if
else
elif / else if
while
for
for (item in list)
```

Example:

```py
for (item in items)
{
    println(item)
}

if(x == 1)
{
   return 1
}
else if (x == 2)
{
   return 2
}
else {
   return 0
}
```

---

### Lists

NovaLang supports dynamic lists with basic oop style operations:

```cpp
list a = [12,3,6,78,2]
```

* `a.push(3)`
* `a.pop()`
* `len(a)`
* `a.removeAt(2)`

Lists can also contain nested values.

Example:

```cpp
list numbers = [1, 2, 3]
list nested = [numbers, 10, 20]

println(nested) // will print as: [[1, 2, 3], 10, 20]
```

---

### Printing and Input

Output is handled using `println()`, which supports multiple values and expressions.

```py
println("Score:", score)
```

Input is handled using `input()`.

```py
auto x = input("Enter your name: ")
println(x) # prints the entered text/numbers
```

Theres a small helper library that helps with specific type inputs:
Not neccessary as the user can also use conversion methods like `toInt(input())`

```py
import "input"

println(inputInt("Enter your age "))

println(inputFloat("Enter the value of pi: "))

println(inputBool("Input a bool value (false/true): "))

println(inputString("Enter your name: "))
```

---

### REPL and File Execution

NovaLang can be used both interactively and through source files.

* **REPL** for quick experimentation
* **Script execution** from `.nova` or `.nv` files

---

# Architecture

NovaLang is implemented as a **bytecode interpreted language**.

Execution pipeline:

```
Source Code
     ↓
Parser
     ↓
Abstract Syntax Tree (AST)
     ↓
Bytecode Compiler
     ↓
Virtual Machine
```

This architecture provides several benefits:

* Faster execution than AST interpreters
* Easier future optimization
* Potential for JIT or native compilation later
* Portable execution environment

---

### Modules and Imports

A modular system for organizing and reusing code.

```py
import "math" # imports all available variables/functions into the modules namespace
import { range } from "lists" # imports specified functions or variables to the global scope

println(range(1,6)) # not namespaced as its a specified import

println(math.square(3)) # default namespacing
```

### File I/O

Nova provides simple and intuitive file handling utilities, inspired by Python-like syntax. You can work with files either through **convenience functions** or **file handles** for more control.

```cpp
// Write to a file (creates or overwrites)
write_file("test_output.txt", "Hello from NovaLang!")

// Read the contents of a file
string content = read_file("test_output.txt")
println(content)

// Read a single line from a file
int file = open("test_output.txt", "r") // open returns the file handle ID
if (file > 0) { // if 0 the file does not exists
    string line = read_line(file)
    println(line)
    close(file)
}

// Append to a file
int file = open("test_file.txt", "a") // writing and appending automatically creates the file
write(file, "Append Test")
close(file)

// Read a file line by line
int file = open("test_line_read.txt", "r")
if (file > 0) {
    while (true) {
        string line = read_line(file)
        
        if (line == none) { // EOF 
            break
        }
        
        println(line)
    }
    close(file) 
}

```

# Planned Features

NovaLang is actively evolving. Planned features include:

---

### Maps / Dictionaries

Key-value data structures for more flexible data storage.

```
user["name"] = "Nova"
```

---

### Networking Support

Built-in support for:

* HTTP
* TCP
* UDP

This will allow NovaLang to interact with APIs, services, and network devices.

---

### Interpreter Optimization

Future improvements may include:

* Bytecode optimizations
* Improved memory management
* Faster execution engine
* Possible native compilation

---

### Expanded Standard Library

Additional utilities are planned, including:

* Advanced list functions
* String utilities
* Math libraries
* System and networking helpers

---

# Philosophy

NovaLang focuses on:

* **Ease of use**
* **rapid prototyping**
* **Intuitive, readable syntax inspired by Python**
* **Typed variables, structs, and functions like C#**
* **gradual typing**
* **safe experimentation**

The goal is to create a language that feels simple like scripting languages while remaining powerful enough for advanced experimentation with automation, and AI systems.
