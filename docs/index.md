Note: The documentation is outdated.  Help in keeping this up to date is always appreciated.  The changelog entries at the end of this page serve as a reminder what still needs to be documented.

Easystroke is a gesture-recognition program, which can execute predefined actions based on "gestures" that you draw on the screen. To see what this is all about, start easystroke and begin drawing a few gestures by holding down the middle mouse button and moving your mouse/pen around. (Peek ahead a little bit if the middle button is inconvenient for you.) Right now, nothing happens yet when you draw a gesture, so let's define some actions next.

Say you want to use gestures to switch tabs in Firefox. 

1. Click on the tray icon to open the program's configuration dialog 
1. Click _Add Action_ to define your first action. An editable text field appears showing the preliminary name of the gesture, _Gesture 1_. 
1. Change the name to _tab-left_, 
1. Select _Key_ in the type column 
1. Press the key combination that goes one tab to the left, namely _Ctrl+Page Up_ (you could also use _Ctrl+Left_, but that won't work in some other apps such as gnome-terminal). 
1. Click on _Record Stroke_, hold down the middle button and move your mouse to the left. Of course you could have just as well used any other shape, including something curvy. 

A _tab-right_ gesture can be defined in the same way, this time moving the mouse to the right (the key combination is _Ctrl+Page Down_, by the way). If you open a Firefox window with a bunch of open tabs, you can now switch tabs just by drawing the gestures you just defined, without having to move you mouse to the other end of the screen).

The rest of the document describes the program's options in more detail. Note that preferences are always saved instantly, so you don't need to worry about losing your newly-recorded strokes if there is a crash. You can quit easystroke by right-clicking on the tray icon and selecting _Quit_.

# Action types
[[Image(MiscWikiFiles:easystroke-actions.png)]]

This is the list of strokes and their associated actions. Adding and deleting lines and recording strokes is straightforward, but let's have a look at the different actions that are currently supported

## Command

Executes a shell command. For example, you could use the tool _xte_ (from the Debian/Ubuntu package _xautomation_) to emulate an arbitrary sequence of key presses (but see below for single key presses).

## Key

Emulates a single key press, possibly with modifiers held down.

## Text

Inserts a fixed text into the application.  For characters that are not associated with any key on the keyboard, this requires working Unicode input (which generally present in gtk applications).

## Ignore

Sends the next "stroke" directly to the application. An useful feature of this action is that it can also hold down a modifier for the next thing you do with your mouse: To set this up, click on the entry in the _Argument_ column and hold down the modifier(s) and in arbitrary key. So for example, in most window managers, you can move or resize windows using the _move_ action in the screenshot above.

## Scroll

Allows you to emulate a scroll wheel by moving your mouse/pen up or down. The action is active until you press and release any button.  You can define an additional modifiers to be held down in the same way as for _Ignore_.

## Button

Emulates a button press, again possibly with modifiers. If you need to be able to move the pointer while the button is held down, consider using the 'Ignore' action or [FeatureAdvancedGestures Advanced Gestures].

## Misc
  * 'Unminimize' undoes the last minimize actions
  * 'Show/Hide' brings up or hides the configuration dialog
  * 'Disable (Enable)' disables (or enables if invoked from the command line) the program.

[[Image(MiscWikiFiles:easystroke-apps.png)]]

# Application and Groups 
By default, gestures work in any application (unless the application is disabled entirely, which can be set up on the _Preferences_ tab).  This feature allows you to set up easystroke to behave differently depending on which application the cursor is in.  To use the feature, simply open the _Applications_ expander on the _Actions_ tab.  You can maintain a list of applications, whose behavior differs from the default by using the _Add Application_ and _Remove Application_ buttons.  The special entry _Default_ refers to all other applications.  To edit the list of gestures for an application simply click on the application.

It is important to understand how application-dependent gestures are stored:  rather than maintaining a list of actions for each application, easystroke only stores the *difference* to its parent node _Default_.  This means that all default actions are available in your applications, and if you add or delete a gesture in the _Default_ actions, this change will be reflected in all applications.  On the other hand, adding, changing or deleting gestures for a specific application never affects the list of default actions in any way, and you can revert to the default at any time using the _Reset Action(s)_ button.  You can even bring actions back from the dead by enabling the _Show deleted rows_ check box and then resetting the deleted actions.  It is easy to tell which actions have been changed compared with the default: changed entries are rendered bold; deleted actions appear grayed out.

The power of this approach is best illustrated by an example:  The screenshot above shows some modifications that were made to the action list for the gnome-terminal application.  Most applications with a tabbed interface use Ctrl+T as a shortcut for opening a tab, but gnome-terminal uses the non-standard Shift+Ctrl+T combination, so that the user can use Ctrl+T to interact with terminal applications.  In easystroke, we can simply change the key combination for gnome-terminal to Shift+Ctrl+T without affecting any other applications.  Similarly, gnome-terminal doesn't support to reopen a closed tab, so we can simply delete the action from the list.

If you'd like several application to be have in a similar way, you may combine them to an _Application Group_.  All applications belonging to the application group inherit their gestures from the group, which in turn inherits its actions from its parent.  Easystroke allows arbitrary trees (with root _Default_) as its hierarchy for applications and groups.

[[Image(MiscWikiFiles:easystroke-preferences.png)]]

# Behavior

'Gesture Button' allows you to choose which button strokes are performed with. The default is the middle button as this is the button that corresponds to the side switch on tablet pens. Note that the left mouse button (on a touch screen or a passive digitizer) works reasonably well, too.

[Advanced Gestures](FeatureAdvancedGestures) have their own documentation page.

Since it is hard to perform clicks without any motion on most pointing devices and since it is also difficult to reliably recognize gestures that are too short, easystroke will only treat motions as gestures that have a certain minimum length. This length can be configured using the 'Treat it as a click if the cursor isn't moved more than this many pixels away from the original position' option.

If you enable the 'Accept gestures when menus are shown' option, easystroke will allow gesturing even when the server is grabbed (which happens when applications display menus and in a few other situations: many compiz plugins do this, too). Note that any clicks you make during a server grab will be passed to the grabbing application, which might result in unexpected behavior, especially if the gesture button is set to the left button.

In 'Timeout profile' profile combo box, you can set up how fast gestures are timing out, or completely turn off timeouts. Note that you can get advanced timeout options if you start easystroke in experimental mode ("easystroke -e").

# Appearance

The following three methods are available to show strokes on the screen:
  * _None_ will disable any feedback when drawing a stroke
  * _Default_ will try to use the best method available on your system.  On composited desktops this draws (anti-aliased and subpixel-aware) strokes on transparent windows, otherwise it will fall back to the XShape method (see below).
  * _XShape_ uses the (old) XShape extension to create a window whose shape is the stroke. This works well on non-composited desktops unless your strokes are very long.
  * _Annotate (Compiz)_ communicates with compiz's annotate plugin via compiz. In order for this to work, you need to enable both the 'annotate' and the 'dbus' plugin. Color, thickness and transparency of strokes can be configured in the compizconfig settings manager.
  * _Fire (compiz)_ Draws gestures as fire. Requires the firepaint and dbus compiz plugins to be enabled and needs a very recent firepaint version from compiz fusion git.
  * _Water (compiz)_ Uses compiz' water plugin to draw gestures.

If the 'Show popups' option is enabled a small popup will be shown whenever a gesture has been successfully identified. The 'optimized for left-handed operation' option will cause the popup to be shown to the left of the cursor so that left-handed Tablet PC users will be able to see it.

# Tablet Options

You should enable the 'Work around timing issues' options if you have a Tablet PC or a wacom tablet and you intend to use the [FeatureAdvancedGestures Advanced Gestures] feature. This will do the right thing if two click events come in at the same time even though you probably clicked the side switch first.

If the 'Stay in scroll and ignore mode as long as the pen is within range' option is enabled, scroll and ignore mode stay activated until the pen is lifted far enough away from the screen. This is only available on devices that support proximity events such as tablets or Tablet PCs.

The 'Abort stroke if pressure exceeds the number on the left' option allows you to abort gestures by exerting a certain pressure on the pen. This is only available on devices that support pressure such as tablets or Tablet PCs.

# Exceptions

'Exceptions' is a list of applications where easystroke is disabled. For example, you could add 'xournal' to the list in order to be able to use the tablet button as an eraser.

[[Image(MiscWikiFiles:easystroke-advanced.png)]]


[[Image(MiscWikiFiles:easystroke-history.png)]]

A common problem with gesture applications is the lack of feedback and transparency: If something goes wrong it's hard to tell what action was executed and why and how you can prevent a similar thing from happening in the future. The _Statistics_ tab tries to alleviate this problem by showing a history of the last few strokes and how close easystroke judged them to be to the strokes in the actions list. This might give you a clue which strokes you might have to re-record or replace by something totally different.

Moreover, the matrix button will show you a complete table of how close the strokes in your database are to each other. It creates a file 'strokes.pdf' in the /tmp directory and then calls evince on it.

# Experimental Mode

For a small project like easystroke, it makes little sense to split development up into a stable and a development tree. Nevertheless, some features may take a little more time to develop, be of questionable usefulness or too poorly documented even for my tastes. I'd also like to release early, release often. That's why I've relegated these features to an _experimental mode_, which can only be enabled on the command line by supplying the parameter '-e'. So if you're curious what this is about, be sure to check it out. Don't worry, your computer won't blow up. Comments on whether any of the experimental features are indispensable or completely useless or how they could be improved are always appreciated.

Finally, a short description of the current experimental features:
* Minimum speed: Specifies the minimum speed that you need to move the pointer with in order for the gesture not to time out. If you don't move the for the amount of time that 'Timeout' is set to, the click is passed along to the application immediately.