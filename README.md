# boredserf - simple web browser
Manifestish summary: your web browser should be like a bored serf. Perhaps the
time for pitchforks and democracy hasn't arrived yet, but we should still do
what we can to put the power of the web back into the hands of the people.

Alternative explainer: of these three things -- coastal waves, your surfboard,
and what you do with the two of them -- which one would you want to
be boring? The tool you use to browse the web should be the same.

Geekspeak summary: boredserf is a browser built on the WebKitGTK[0] engine with
the idea that the best programs are pipelines of simpler programs composed to
suit the need at hand.

## Status
Ugly. Ugly ugly ugly. Stay away unless you're ready for things to go wrong.

boredserf is currently little more than a fork of surf[1]. boredserf no longer
maintains surf's philosophy and style, yet neither has it implemented its own.
As the maps might once have said: There Be Bugs, Likely Big Ones With Wings And
Pointy Teeth.

Users of commercial operating systems need not apply.

### Content Blocker
Mostly functioning:
- WebKit-based (highly private and fast) content filtering
- In-browser editor of content filtering rules
- Stripping of inline content (which WebKit's tools do not do)

Still needed:
- Robust read/write of rule configuration file format
- Analysis of rule inheritance, and editing of them
- Applying inherited rules to inline content stripper
- Handling of regular-expression-rules in the editor

### Software-As-An-API
Begun:
- Design: each browser function will have a designated handler that accepts
  char* input with clear parsing.
- Implementation: a meta-function takes two char* strings and runs one as a
  shell command with the other as standard input to that command. The resulting
  standard input is returned to the function handler.

Still needed:
- Identification and specification of all features, not just the prominent
  ones.
- Proof-of-concept by porting existing features.
- Implementation of everything.

### Architecture/Implementation/Code Style
Begun:
- Concept: the primary heuristic for how to implement necessary functionality
  is to make choices that improve the ability of this codebase to help other
  people improve their programming skills.

Still needed:
- Re-style existing code
- Add minimal guards and messages
- Rewrite parts that need it

## Why fork?
In the opinion of the boredserf originator, three things were necessary to make
the browser usable but were incompatible with the suckless philosophy.[2]

The first need was to implement a content filter. The web is not safe without
one; the web is more comfortable and productive when the user is able to find
the content they want with minimal abuse. A basic content filter represented
one-third of a patched surf's codebase; with maturity, it will likely become
larger than the essential browser-engine-window that surf provides. As suckless
is explicitly opposed to adding lines of code regardless of their purpose, this
tool appears incompatible with surf. We hope surf will someday achieve its
vision of a suckless content filter, but this is not it.

The second need is to allow concurrent and interleaved feature development.
surf development occurs via patches, typically git commits via email. Many
patches have bit rot; are mutually incompatible; substantially overlap in
purpose; and make it difficult to offer even unrelated contributions.
Maintaining all features in a single repository would ease discovery, testing,
depreciation, and removal. We know suckless has good reasons for maintaining
this development approach, however it is best suited to the small, simple
programs that align with their philosophy -- not large programs such as even a
minimal implementation of a WebKitGTK program.

The third need is to improve code clarity. As with process, above, the costs
and benefits of different stylistic choices change when the scope which the
code must cover grows too large to meet with a minimal-code philosophy. Within
a collection small programs that use consistent libraries and idioms, a concise
style is expressive and powerful. Within a large program that uses diverse
libraries and which must deploy varying idioms to use them, a verbose style is
necessary to avoid confusion and exhaustion of mental resources. This might be
expressed as the difference between knowledge-based and ignorance-based coding.
Small utilities seem to be best written in such a way that the contributor must
gain a holistic understanding of the code to meaningfully improve it; this is
readily attainable and limits problems created by different people making
equally reasonable interpretations of the pieces they look at. Large programs
seem to be best written in such a way that the contributor need not know
anything beyond the adjacent interfaces; the limited breadth of study required
of the contributor comes at the cost of increased verbosity in comment and
code. boredserf is too large a program to expect any contributor to comprehend
the whole thing.

Perhaps, if boredserf achieves its vision of becoming little more than an API,
it can return to the suckless fold.

## Requirements
In order to build boredserf you need GTK+ and Webkit/GTK+ header files.

In order to use the functionality of the url-bar, also install dmenu[2].

## Installation
Edit config.mk to match your local setup (boredserf is installed into the
/usr/local namespace by default).

Afterwards enter the following command to build and install boredserf (if
necessary as root):

```
make clean install
```

## Running boredserf
run
```
boredserf [URI]
```

See the manpage for further options.

## Running boredserf in tabbed
For running boredserf in tabbed[3] there is a script included in the
distribution, which is run like this:
```
boredserf-open.sh [URI]
```

Further invocations of the script will run boredserf with the specified URI in
this instance of tabbed.

[0] https://webkitgtk.org/
[1] https://surf.suckless.org/
[2] https://suckless.org/philosophy/
[3] https://tools.suckless.org/dmenu
[4] https://tools.suckless.org/tabbed

