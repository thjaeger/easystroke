  * [2018-12-05] Maintenance Update. As development has stalled, and the original Author is not available, but the program is still widely used, an update in terms of bugfixing and maintenance of the code has been attempted. See [BuildInstructions](https://github.com/thjaeger/easystroke/wiki/BuildInstructions) for more information.
  * [2013-03-27] Released 0.6.0 This release contains few user-visible changes beside a few bugfixes.  Easystroke has been ported to gtk3 and event handling code has been refactored in anticipation of touch support.  Unfortunately, touch grab support in the X server is still too fragile for easystroke to be able to take advantage of it (you can preview touch support on the 'touch' branch).
  * [2012-10-06] Released 0.5.6 This release incorporates fixes for most of the issues that have come up over the last year or so.  It also adds Hungarian translations.  Stay tuned, 0.6.0 with gtk3 and improved touch support will be released soon.  The 0.5 branch will not see any new features, only bugfixes if necessary.
  * [2011-08-16] Released 0.5.5.1 This fixes a build failure in 0.5.5.
  * [2011-08-16] Released 0.5.5 This release adds per-device timeout settings. An extra click is now required for button actions to improve precision and scroll actions use XI2 Raw events so that the cursor is not confined to the screen.  The release also includes various bugfixes and adds Catalan, Finnish, Korean and Traditional Chinese translations.
  * [2010-07-27] Released 0.5.4 Just a few minor bug fixes.
  * [2010-02-13] Released 0.5.3 This release adds an option to move the cursor back to the original position after each gesture and fixes a crash when a gesture times out during recording, along with a few minor bugs.
  * [2010-01-09] Released 0.4.11 This release only fixes one bug that would cause a wrong key to be emitted under certain circumstances. 
  * [2010-01-02] Released 0.5.2 This release adds Hebrew translations and should improve keystroke handling.
  * [2009-11-19] Released 0.4.10 This is a bugfix release for the non-xi2 branch of easystroke.  It also adds Hebrew translations.
  * [2009-10-27] Released 0.5.0 This release is a port of easystroke to XI 2 (it doesn't add any new features).  You need a recent X server (at least 1.7) to run this version.  Most people will probably want to stick with 0.4.9 for now.
  * [2009-08-19] Released 0.4.9 This release fixes a bug regarding application groups and adds Polish translations.
  * [2009-08-15] Released 0.4.8 This release fixes a long-standing bug that would prevent easystroke from working when only application-dependent gestures were defined.
  * [2009-07-05] Released 0.4.7 This release adds a workaround an (1.6) X server bug causing problems with vertical gestures on some devices.
  * [2009-06-16] Released 0.4.6 This release adds Russian translations and works around two issues on karmic and amd64 arch.
  * [2009-06-01] Released 0.4.5 This release fixes one crash-bug and adds Chinese translations.
  * [2009-05-12] Released 0.4.4 This fixes two annoying bugs in 0.4.3
  * [2009-05-09] Released 0.4.3 This is mostly a bugfix release, but it also improves drag-and-drop behavior
  * [2009-05-09] Released 0.4.2 This is a bugfix-only version of 0.4.3 that I accidentally uploaded, please ignore.
  * [2009-03-16] Released 0.4.1.1 This is a bugfix release.
  * [2009-02-20] Released 0.4.1 This release introduces a new stroke-matching algorithm, adds click-and-hold gestures and contains various bug fixes.
  * [2009-02-06] Released 0.4.0 This version adds support for the upcoming X Server 1.6, comes with Czech, German, Italian and Spanish translations and makes it possible to use multiple gesture buttons.
  * [2008-12-22] Released 0.3.1 We now have a much better method for drawing strokes on composited desktops, but this release also contains a few bugfixes and an option to autostart the program.
  * [2008-11-07] Released 0.3.0 This release introduces many new features, most notably application-dependent gestures. See the [changelog](http://github.com/thjaeger/easystroke/tree/master%2Fchangelog?raw=true) for details
  * [2008-09-18] Released 0.2.2.1 This is a bug fix release, resolving a few minor issues.
  * [2008-08-17] Released 0.2.2 This version features improved visual feedback, configurable gesture timeout, better pointer tracking and many minor improvements.
  * [2008-08-06] Released 0.2.1.1. This just adds license information to the source files in order to facilitate Ubuntu packaging. An update is not necessary.
  * [2008-08-03] Released 0.2.1, correcting some silly Makefile mistakes.
  * [2008-08-03] Released 0.2.0. This version contains many tablet-related improvements and introduces [FeatureAdvancedGestures]. Please allow a few days for the documentation to be updated to the current version.
  * [2008-06-22] Released 0.1.2. Fixes a few minor UI glitches.
  * [2008-06-19] Released 0.1.1. This version introduces [FeatureClickDuringStroke], which allows you to emulate a mouse click by clicking a second button during a stroke (which can essentially turn a one-button tablet pen into three-button mouse).
  * [2008-06-14] Released 0.1. This is first public release.


2013-03-27 Release 0.6.0
	* Update easystroke to gtk3
	* Refactor event handling code
	* Port CellRendererTextish to vala to make it work again
	* Make unminimize work right

2012-10-06 Release 0.5.6
	* Keycode translation improvements
	* Allow gestures to be distinguished by modifiers
	* Improve detection current application
	* Minor UI improvements
	* Make Bell work again
	* Add Hungarian translations

2011-08-16 Released 0.5.5.1
	* Fix build failure due to missing import

2011-08-16 Released 0.5.5
	* Different timeout settings for individual devices
	* Change the way button action work to make them more touchscreen
	  friendly
	* Improve Application detection
	* Use raw events for scrolling
	* Add Catalan, Finnish, Korean and Traditional Chinese translations
	* Make fire and water work with compiz 0.9
	* Add options to not use dbus and to start easystroke disabled

2010-07-27 Released 0.5.4
	* Don't start with the config dialog unless -g is passed on the
	  command line
	* Key action and cursor position fixes
	* Update Russian translations

2010-02-13 Released 0.5.3
	* Fix a crash when recording gestures
	* Report window manager frames as such in the application list
	* Improve the recognition algorithm
	* Add an option to move the cursor back to the original position

2010-01-02 Released 0.5.2
	* Add Hebrew Translations
	* Fix crasher bug for devices with relative 3rd axis
	* Improve keycode handling

2009-10-27 Released 0.5.0
	* Port easystroke to XI2, simplifying much of the code.  Note that
	  this version of easystroke will not work on X servers < 1.7.

2009-08-19 Released 0.4.9
	* Fix a bug regarding application groups
	* Add Polish translations

2009-08-15 Released 0.4.8
	* Fix the use-case where only application-dependent gestures are defined
	* Set default gesture trail width to 3
	* Update Russian translations

2009-07-05 Released 0.4.7
	* Work around an X server bug causing problems with vertical gestures
	* Update Russian translations

2009-06-16 Released 0.4.6
	* Switch from -Os to -O2 to work around gcc bug
	* Add Russian translations
	* Build fix for karmic

2009-06-01 Released 0.4.5
	* Fix bug that caused easystroke to crash on start up if
	  there were devices that couldn't be opened
	* Add Chinese translations

2009-05-12 Released 0.4.4
	* Fix bug that made it impossible to record gestures if there were no
	  default actions
	* Make editing commands possible again

2009-05-09 Released 0.4.3
	* Improve DND behavior

2009-05-09 Released 0.4.2
	* XInput bugfixes
	* French & Japanese translations and various translations updates

2009-03-16 Released 0.4.1.1
	* Allow button remapping when XInput is disabled
	* Various bugfixes
	* Allow easystroke to only be enabled for specific applications
	* Option to hide OSD
	* Translation updates

2009-02-20 Released 0.4.1
	* New stroke matching algorithm
	* New timeout algorithm
	* Click & Hold gestures
	* Kill radius property
	* Set environment variables containing start/end point
	* Fix 'Command' action
	* Option to show last gesture in tray
	* Option to change scroll speed/direction

2009-02-08  Released 0.4.0
	* Support for xserver-1.6
	* Additional gesture buttons
	* Instant Gestures
	* Add Czech, German, Italian and Spanish translations
	* Add 'Text' action
	* Add "Timeout-Advanced" gestures
	* Add conservative timeout profile, rename conservative to default
	* Disable easystroke through middle-click 
	* Show a different icon when disabled
	* Show an 'icon hidden' warning when appropriate
	* Never show clicks
	* Display a warning for 'click' gestures
	* Save preferences in version-specific file
	* Option to not show popups on advanced gestures
	* Don't abort gestures when Escape is pressed, as this was causing
	  lock-ups

2008-12-22  Released 0.3.1
	* Better method for drawing strokes on a composited desktop
	* Several bug-fixes
	* Autostart option

2008-11-07  Released 0.3.0
	* application-dependent gestures
	* application-dependent gesture button
	* Timeout gestures
	* disable easystroke for specific devices
	* easystroke send "foo" to excecute action foo from the command line
	* disable easystroke using the tray menu or by a gesture
	* 3 new actions:
		+ "Unminimize" undoes the last minimize actinos
		+ "Show/Hide" brings up or hides the configuration dialog
		+ "Disable (Enable)" disables (or enables if invoked from the
		  command line) the program.
	* action list now reorderable
	* hot-plugging support
	* option to activate gestures with any modifier combination held down
	* option to hide tray icon
	* higher quality gesture previews
	* Feedback in the tray icon whether the gesture succeeded or not
	* Ring the bell on failed gestures
	* Blue popup to notify user of scroll and ignore mode
	* added support for compiz water plugin
	* option to change the stroke color
	* dropped support for some advanced features when xinput is unavailable
	* -c option to start easystroke with config window open
	* dropped -n (no gui) option

2008-09-18  Released 0.2.2.1
	* bug fix release, resolvs a few minor issues

2008-08-17  Released 0.2.2
	* improved visual feedback
	* configurable gesture timeout
	* better pointer tracking
	* many minor improvements

2008-08-06  Released 0.2.1.1.
	* add license information to the source files

2008-08-03  Released 0.2.1, correcting some silly Makefile mistakes.

2008-08-03  Released 0.2.0.
	* many tablet-related improvements
	* advanced gestures

2008-06-22  Released 0.1.2.
	* Fixes a few minor UI glitches.

2008-06-19  Released 0.1.1.
	* "click during stroke"
	  This feature allows you to emulate a mouse click by clicking a second
	  button during a stroke (which can essentially turn a one-button tablet pen
	  into three-button mouse).

2008-06-14  Released 0.1
	* First public release.
