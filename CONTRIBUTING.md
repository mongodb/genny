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

Public headers must have a `.hpp` suffix. Private headers must have a
`.hh` suffix.

General structure:

-   Include Guard (`#pragma once`)
-   System Headers `<vector>` (alphabetical order)
-   Open Namespace `genny`
-   Code
-   Close Namespace `genny`

Example:

```cpp
#pragma once

#include <vector>
#include <genny/blah.hpp>

namespace genny {
// Declarations
// Inline Implementations
}  // namespace genny
```

## Class Declarations

Guidelines:

-   Blank line at beginning and end of class declaration
-   Public section up top / private at bottom
-   Lifecycle methods first (see rules above)
-   Private Member Ordering
    -   Friendships
    -   Private Constructors
    -   Private Methods
    -   Private Variables

Example:

```cpp
class foo {

    public:
      foo();

      foo(foo&& other) noexcept;
      foo& operator=(foo&& other) noexcept;

      ~foo();

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
