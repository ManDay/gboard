GBoard - Simple, flexible, on-screen keyboard

Author: Cedric Sodhi <manday@gmx.net>

URL: github.com/ManDay/gboard

=== LICENSE

See file COPYING (blindly copied from the GTK+-3.0 tarball due to my
current lack of internet access).

=== CONTENTS

Contains the on-screen keyboard, labeled "GBoard", an IM-Module for
GTK+-3.0 (practically dysfunctional at the moment) and supplementary
files.

=== DEPENDENCIES

Depends on GTK+-3.4 or above, the X11 XTest extension (comes with X.org
on practically every distribution), and CMake.

=== INSTALL

GBoard uses the CMake Build-System. In order to build and install the
software create a new, empty directory,

$ mkdir gboard

changed into it

$ cd gboard

and instruct CMake to prepare the build,

$ cmake /path/to/sourcecode/of/gboard

where above "/path/to/sourcecode/of/gboard" is the path to the directory
in which this README.txt and the auxiliary CMakeLists.txt file can be
found. If you have ccmake installed (the Curses frontend to cmake), you
may run

$ ccmake .

to interactively view and edit build-time configuration. In order to
compile and install the software, run

$ make install

With the default configuration, GBoard will install its main executable,
i.e. the keyboard and all associated resources, such as an icon and a
small set of layout-files, and an IM-Module for GTK+-3.0.

=== CONTENTS

GBoard is a keyboard which can be operated either through commandline or
over DBus.

The GTK IM Module links GBoard to GTK entry widgets.

Layout files define layouts for GBoard.

Emitter (currently there exists only the X11 emitter) provide backends
to GBoard to perform whichever operation is desired. To illustrate the
generality of this, one may write a layout file for a piano and use a
sound emitter as the backend. The X11 emitter performs the canonical
operations of the keyboard, that is emitting keypresses.

The DBus Service File can be used to auto-start GBoard over DBus.

=== GBOARD EXECUTABLE

GBoard is started with

$ gboard

Note that, if gboard is installed into a non standardized path, the
environment will have to adjusted in order to allow GSettings to work,
meaning the environment variable XDG_DATA_DIRS will have to contain the
path to GBoard's installed "share" directory and usually also to the
system's "share directory", e.g.:

XDG_DATA_DIRS="/home/desrt/gboard/share:/usr/share"

GBoard supports the following switches on command line

-f
--force
 Reload specified layout files and do not read them from cache

-s
--show
 Show the keyboard

-h
--hide
 Hide the keyboard

followed by an arbitrary number of layout files, all of which will be
loaded and cached and the last is activated.

GBoard will appear in the system tray area (if such an area is
available). A left click onto the tray icon will toggle GBoard's
visibility.

=== GTK IM MODULE

What the IM Module should do and does well is pop up a little button
next to each highlighted entry, offering the user to bring up GBoard
over DBus (Note: There is no actual IM functionality intended. The IM
Module just serves as a way to "hook into" the user's highlighting an
entry). Although this works flawlessly, the IM Module remains
practically unusable because I could not figure out how to return
control to whichever IM Module provides normal input, meaning that as
soon as GBoard's IM Module fires up, the entry no longer responds to
keypresses (including those from GBoard).

=== LAYOUT FILES

Layouts can be loaded via command line (see above). Layout files are
bound to specific emitters, as they specify emitter-specific keynames
for individual keys and may generally depend onto emitter specific
behaviour (X11 being the canonical example).

A layout is best understood in terms of the implementation. A layout is
printed onto a rectangular grid. The smallest (squared) key takes up
four cells (two rows and two columns) on that grid. The smalles
(squared) space takes up one cell. Keys and spaces may span more than
the minimal amount of cells, but they are always rectangular.

A layout file specifies the position and size of the physical keys in
terms of "Keygroups", and the different functions a phyisical key may
provide as "Keys".

As an example, the physical key "a"/"A" is represented as one Keygroup
(usually two times two cells) with two contained keys, namely "a" and
"A".

The "layout file" is based upon the following syntax in BNF. Spaces are
not strictly specified. For correct whitespace usage see the layouts
which are shipped with GBoard. Whitespace is mostly ignored.

<layoutfile> ::= <row> <linebreak> <layoutfile> | <row>
<row> := <keygroups>
<keygroups> ::= <keygroup> | <keygroups> <keygroup>
<keygroup> ::= "{}" |
               "{" <keygroupcontents> "}" |
							 "{" <keygroupcontents> <extender> "}"
<extender> ::= <rowextension> |
               <rowextension> <colextension> |
							 <colextension>
<rowextension> ::= "_" | "_" <rowextension>
<colextension> ::= "}" | "}" <colextension>
<keygroupcontents> ::= <key> | <key> <keygroupcontents>
<key> ::= <filter> <keycontents> | <keycontents>
<keycontents> ::= "{" <action> "}" | "{" <action> <label> "}"
<action> ::= <keycode> |
             "(" <exec> ")" |
						 "{" <modifier> "}" |
						 "{" <keycode> <modifier> "}"
<label> ::= <text> | "(" image ")"

The meaning of terminal symbols should be derived from the following
explanations.

Ocurrences of braces (unfortunately not parentheses) where they are not
intended to bear syntactical meaning can be escaped by prefixing them
with a single backslash, so can be backslashes and linebreaks. Three
examples for valid keygroups, including the use of escaping:

{ {keycodeA My unfiltered key} filterX{keycodeN My \{X\}-filtered key} }
{ {(shutdown -h now) My shutdown key} filterY{keycodeL} }
{ {{filterX} X-Modifier-Key} {keycodeR (colorfulR.png)} }

Each row in the layout file represents exactly one row in the grid. This
means that if you use a key in that row, which is necessarily higher
than one row (remember: the smalles key is two rows high), you will
most likely want an empty line in order not to "run into" that key while
processing the next line.

A keygroup, satisfying the above syntax is two times two cells large if
it contains content, or one cell large if not. In either case, it can be
streched out towards the right or the bottom by using extenders.
Underscores specify extension towards the bottom, "superflous" closing
braces "}" extension towards the right. A keygroup which spans three
cells in the horizontal direction and six cells in the vertical
direction is conceptually specified as follows:

{ <keygroupcontent> ____}}

This adds fours cells towards the bottom and one towards the right. If
there were no keygroupcontent, the resulting size would be one smaller
in each direction because spaces are naturally smaller than keys.

Each key within a keygroup is of the following format

Filter { Action Label }

Where both Filter and Label may be ommitted. Specifying a filter before
a key means that the key becomes visible if a modifier of the same name
is pressed. The typical character keygroups look thus much like

{ {a} shift{A} }

meaning that the key "A" is displayed if the shift modifier is pressed.
Filters and Modifiers are both subjected to the same principle:

If the first letter is capital "Shift", the modifier or filter is said
to be "sticky". If it's not capital "shift", the modifier or filter is
said to be non-sticky.

Non-sticky modifiers "elevate" keys with either the according sticky, or
non-sticky filter. Sticky modifiers elevate only those keys with a
sticky filter. If both, sticky and non-sticky modifier are pressed, the
modifier is said to be "inverted", meaning that only those keys which do
not match with the sticky modifier will get elevated.

This reflects the behaviour you experience with Capslock (sticky
"Shift") and Shift (non-sticky "shift").

If the label of a key is omitted, it defaults to the Keycode. If the
action is enclosed in parentheses, it's executed in a child process. If
the label is enclosed in parentheses, GBoard will attempt to load a
picture with that path.

After all, the example layouts should provide you with an idea what is
possible. In particular, note that exec-keys may be used to change the
layout on the fly by executing

$ gboard someOtherLayout.gboard

which may be used for things such as popping up the Numblock or controls
for simulating mouse controls (compare Ubuntu's "onBoard") by
subsequently executing something such as xdotool.

=== TODO

Eventually, GBoard shall provide not only a keyboard but also a
handwriting recognizer (inspired by Cellwriter) and, if desired, change
automatically between the sheet for handwriting and the keyboard,
depending on the current device, with which the mouse is controlled.

Until then, a small list of rather trivial things remains to be done on
GBoard's currently available keyboard, namely:

* Improve the X11-Emitter to keep track of pressed modifier keys
  (internally), so that it will be able to emit any keysym irrespective
	of the currently pressed modifiers.

* Optional: Add Wayland emitter.

* And a manpage, perhaps. Although I'm a big groff enthusiast, I have no
	time for writing one right now.
