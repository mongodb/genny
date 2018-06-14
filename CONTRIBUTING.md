Contributing Guidelines
=======================

## Commit Messages

If a pull-request addresses a JIRA ticket, for a single-commit PR,
prefix the subject line with the ticket ID. (For a multi-commit PR, we
will add the ID later when we squash or merge it.)

> ABC-883 Add commit message conventions to CONTRIBUTING.md

Capitalize subject lines and don't use a trailing period. Keep the
subject at most 70 characters long. Use active voice! Imagine this
preamble to get your phrasing right:

> *If applied, this commit will...* %%your subject line%%

See Chris Beams' [How to write a git commit message](b) for more good
guidelines to follow.

## Lifecycle Methods

-   default-or-argument-bearing 'user' constructors

-   declaration-or-deletion-of-move-constructor
-   declaration-or-deletion-of-move-assignment-operator

-   declaration-or-deletion-of-copy-constructor
-   declaration-or-deletion-of-copy-assignment-operator

-   declaration-of-dtor

## Headers

Naming and bureaucracy:

-    Public (exposed) header files belong in `include` directories.
-    Public headers must have a `.hpp` suffix.
-    Private headers must have a `.hh` suffix.
-    Header file names are in `snake_case`.
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

Guidelines:

-   Blank line at beginning and end of class declaration
-   Class names in `PascalCase`
-   Public section up top / private at bottom
-   Lifecycle methods first (see rules above)
-   Private Member Ordering
    -   Friendships
    -   Private Constructors
    -   Private Methods
    -   Private Variables (leading underscore and `lowerCamelCase`)

Example:

```cpp
class Foo {

    public:
      Foo();

      Foo(Foo&& other) noexcept;
      Foo& operator=(Foo&& other) noexcept;

      ~Foo();

    private:
      friend baz;

      class impl;
      std::unique_ptr<impl> _impl;

};
```

## Inlines

-   Define outside of class declaration
-   Specify inline keyword in declaration and definition (for clarity)

## Relational Operators

-   Prefer to use free functions
