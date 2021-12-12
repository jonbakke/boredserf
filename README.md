# boredserf - simple webkit-based browser
boredserf is a web browser that aims to be as exciting as a bored serf. Based on WebKitGTK[0], boredserf provides support for modern webpages without the bulk of modern browsers.

## Purpose
At the moment, boredserf is little more than a fork of `surf`[1], plus a few
patches, plus a rather immature content blocker.

This project has three goals that appear incompatible with surf:
- Legible code. boredserf aims to be written in a way that is easy to maintain, improve, and learn from.
- Chain-of-small-tools. boredserf will focus on being a good citizen among other programs. All interaction and non-browser-engine features should be easy to replace to meet each individual's needs in every circumstance.
- Ready to run. boredserf intends to be suitable for daily use with minimal configuration.

## Status
Ugly. Ugly ugly ugly. Stay away unless you're ready for things to go wrong.

The first need was to implement the content blocker. That is approaching rough usability, but core features are still missing.

The second need is to move away from adhoc code patches for any new feature, toward a stable plug-in API. The current concept is to have interfaces and features defined by three strings: one (shell) command to run with arguments, one for input given to that command, and one for the output returned by that command. Details TBD.

The third need is to make the code more robust with increased whitespace and comments as well as guards against out-of-bounds parameters and more informative error handling.

Features will be added along the way to allow this to be a daily-use browser all along.

## Requirements
In order to build boredserf you need GTK+ and Webkit/GTK+ header files.

In order to use the functionality of the url-bar, also install dmenu[2].

## Installation
Edit config.mk to match your local setup (boredserf is installed into
the /usr/local namespace by default).

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
[2] https://tools.suckless.org/dmenu
[3] https://tools.suckless.org/tabbed

