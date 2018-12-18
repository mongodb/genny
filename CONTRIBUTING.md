Contributing Guidelines
=======================

We follow the [MongoDB Server Code Style][msg] except:

-   We use GUID include-guards rather than `#pragma once`
-   File names match class names e.g. `class FooBar` will be found in
    `FooBar.hpp` and `FooBar.cpp` rather than `foo_bar.hpp`.

We have not yet officially brought over the clang-formatter settings
from the server.

[msg]: https://github.com/mongodb/mongo/wiki/Server-Code-Style


## Don't Get Cute

Please avoid:

-   Complicated macros (e.g. `__VA_ARGS__` or `#ifdef` excluding
    include-guards)
-   Macros for syntax tokens (e.g. `class MyClass : PRIVATE_SUBCLASS Y`
    or `namespace SOME_NAMESPACE`)
-   Conditional-compilation (we strive to only use widely-supported
    standard C++ features)
-   Compiler-specific features
-   `std::experimental`
-   Code-generation (e.g. generating C++ from another language, including
    CMake)

Many editors and IDEs have a hard time if code hides lots of things
behind macros or code-generation or otherwise uses cleverness. While
there is a lot of power in macros and code-generation, for the
time-being we are valuing wide editor support and simplicity of
conventional methods.

## Commit Messages

If a pull-request addresses a JIRA ticket, for a single-commit PR,
prefix the subject line with the ticket ID. (For a multi-commit PR, we
will add the ID later when we squash or merge it.)

> ABC-883 Add commit message conventions to CONTRIBUTING.md

Capitalize subject lines, and don't use a trailing period. Keep the
subject at most 70 characters long. Use active voice! Imagine this
preamble to get your phrasing right:

> *If applied, this commit will...* %%your subject line%%

See Chris Beams' [How to write a git commit message](b) for more good
guidelines to follow.

## Headers

Naming and bureaucracy:

-    Public (exposed) header files belong in `include` directories.
-    Public headers must have a `.hpp` suffix.
-    Private headers must have a `.hh` suffix.
-    Header file names are in `snake_case`, but headers defining a single class are `PascalCase`.
-    Class names are in `PascalCase`, and local/member variables are `lowerCamelCase`.
-    Member variables (class properties) have leading underscores.
-    Method parameters do not have leading or trailing underscores.

General header file structure:

-   Include Guard (`#ifndef HEADER_{uuid}_INCLUDED` `#define HEADER_{uuid}_INCLUDED`).
    Use `uuidgen | sed s/-/_/g` to generate `{uuid}` for new files.
-   System Headers `<vector>` (alphabetical order)
-   Blank line
-   Local headers `<genny/foo.hpp>` (alphabetical order)
-   Open Namespace `genny`
-   Code
-   Close Namespace `genny`

(Helpful tip: make a shell alias for the include string:
`alias huuid="echo \"HEADER_\$(uuidgen | sed s/-/_/g)_INCLUDED\""`)

Example header file:

```cpp
#ifndef HEADER_9854B7E9_CAFF_4CD3_8A48_BD5E6A368C96_INCLUDED
#define HEADER_9854B7E9_CAFF_4CD3_8A48_BD5E6A368C96_INCLUDED

#include <string>
#include <vector>

#include <genny/bar.hpp>
#include <genny/foo.hpp>

namespace genny {

// Declarations
// Inline Implementations

}  // namespace genny

#endif  // HEADER_9854B7E9_CAFF_4CD3_8A48_BD5E6A368C96_INCLUDED
```

## Class Declarations

Guideline and ordering:

-   Blank line at beginning and end of class declaration
-   Class names in `PascalCase`
-   Public section up top:

    -   default-or-argument-bearing 'user' constructors
    -   declaration-or-deletion-of-move-constructor
    -   declaration-or-deletion-of-move-assignment-operator

    -   declaration-or-deletion-of-copy-constructor
    -   declaration-or-deletion-of-copy-assignment-operator

    -   declaration-of-dtor

    -   Public methods

-   Passkey/pseudo-private methods within public section
-   Private section on bottom:

    -   Friendships
    -   Private Constructors
    -   Private Methods
    -   Private Variables (leading underscore and `lowerCamelCase`)

-   Blank line at end of class declaration.

Example:

```cpp
#ifndef HEADER_82C204F5_26C2_4B39_9469_9FE242037891_INCLUDED
#define HEADER_82C204F5_26C2_4B39_9469_9FE242037891_INCLUDED

#include <string>
#include <vector>

#include <gennylib/Foo.hpp>

namespace genny {

class Foo {

public:
    Foo();

    Foo(Foo&& other) noexcept;
    Foo& operator=(Foo&& other) noexcept;

    ~Foo();

    std::vector<int> explode(const std::string& y);

private:
    friend class Baz;

    class impl;
    std::unique_ptr<impl> _impl;

};

}  // namespace genny

#endif  // HEADER_82C204F5_26C2_4B39_9469_9FE242037891_INCLUDED
```

## Inlines

-   Define outside of class declaration
-   Specify inline keyword in declaration and definition (for clarity)

## Relational Operators

-   Prefer to use free functions

## Use Scoped Enums

-   Use scoped enums (`enum class`) wherever possible
-   Use `k` prefixes for enum values e.g. `enum class Color { kRed, kBlue };`
