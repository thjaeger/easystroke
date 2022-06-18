Easystroke is a gesture-recognition application for X11. Gestures or strokes are movements that you make with you mouse (or your pen, finger etc.) while holding down a specific mouse button. Easystroke will execute certain actions if it recognizes the stroke; currently easystroke can emulate key presses, execute shell commands, hold down modifiers and emulate a scroll wheel. The program was designed with Tablet PCs in mind and can be used effectively even without access to a keyboard. Easystroke tries to provide an intuitive and efficient user interface, while at the same time being highly configurable and offering many advanced features.

# News
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

# Download

Releases can be found on the [sourceforge download page](http://sourceforge.net/project/showfiles.php?group_id=229797). See the [BuildInstructions Build Instructions] to learn how to build easystroke from source or see below if there are pre-built packages available for your distribution.

## Ubuntu
You can find i386 and amd64 .debs for Ubuntu Karmic and Hardy on the [sourceforge download page](http://sourceforge.net/project/showfiles.php?group_id=229797). These packages, along with packages for Jaunty and Lucid, are also available through the [easystroke Lauchpad PPA](https://launchpad.net/~easystroke/+archive/ppa). Just add the following two lines to your /etc/apt/sources.list file (replace karmic with the verion of ubuntu that you're running).
	
	deb http://ppa.launchpad.net/easystroke/ppa/ubuntu karmic main
	deb-src http://ppa.launchpad.net/easystroke/ppa/ubuntu karmic main
	

## OpenSUSE

Easystroke is part of the openSUSE Contrib repository and the X11:Utilities devel 
repository, see the [openSUSE Build Service](http://software.opensuse.org/search?baseproject=ALL&p=1&q=easystroke) for a list available builds.

# Development

You can fetch the [latest development tree](http://github.com/thjaeger/easystroke/tree/master) using git:
	
	git clone git://github.com/thjaeger/easystroke.git
	

# Documentation

[Documentation](Documentation)

[Tips & Tricks](TipsAndTricks)

[Build Instructions](BuildInstructions)

Easystroke is distributed under the [ISC License](http://github.com/thjaeger/easystroke/tree/master%2FLICENSE?raw=true).

# How can I help out?

If you encounter any bugs or problems with easystroke, please report them, preferably using [trac](https://sourceforge.net/apps/trac/easystroke/report) (be sure to assign the ticket to me (thjaeger), so that I get notified by e-mail). Feature suggestions are also welcome, but note that I only have limited time to implement new features (usually, changes in the X server are enough to keep me busy).

The easystroke website is a wiki that anyone can edit (with the exception of the front page). Please feel free to add any information (tutorials, documentation, etc.) that might be interesting to other users. Work on the [Documentation](Documentation) page is especially appreciated, I don't have the time and patience to keep it up to date.

To find out how to translate easystroke into your native language, see [Translations](Translations).

# Author

Easystroke is being developed by [Thomas Jaeger](mailto:ThJaeger@gmail.com). Comments, suggestions, bug reports, patches and criticism are very welcome!