Configure Tap Zones and use them as buttons and separate scroll wheels. I've detailed it here

Configure Tap Zones- http://ubuntuforums.org/showthread.php?t=1824870
Set up tap zones to perform buttons and scroll wheels-  http://ubuntuforums.org/showthread.php?t=1859936

*Using Easystroke to perform like Scrybe Gesture Workflows — Searching and Shopping*

First install xsel

    sudo apt-get install xsel

we will be using a command type

Example: searching for "obama" in wikipedia
in the command entry type:

	
	google-chrome "http://en.wikipedia.org/w/index.php?title=Special:Search&search=`xsel -p | tr [:space:] +`"
	
	

now choose a gesture, lets choose a shape `w`

highlight a word and perform the gesture and you should be there.

to get any other website search perform a search query "test" in there search engine i,e, youtube 
which gives the result
http://www.youtube.com/results?search_query=test&aq=f

now replace "test" with `xsel -p | tr [:space:] +` (with those little brackets)

then your command will be

	
	google-chrome "http://www.youtube.com/results?search_query=`xsel -p | tr [:space:] +`&aq=f"
	
	

make sure you use the brackets for your url.

any other search engines can be done using this.

# General

## Mapping mouse buttons to keyboard shortcuts

If you have a mouse button that you don't need, you can use easystroke to remap it to any command or keyboard shortcut.  Just add the button as an additional button (or use the gesture button if you don't want to use gestures).  Since you don't want to use the button for gestures, select "Instant Gestures" on the bottom of the button dialog.  To assign an action to the button, just press the button when recording a new gesture.  To assign a modifier to the mouse button, you can use the "Ignore" action type.

## Emulating a scroll wheel using a button

If you don't have a scroll wheel, you can make easystroke emulate one by pressing a button and moving the cursor.  Add the button as an additional button of type "instant gestures" as above, and record a new gesture by clicking the button, then change the action's type to "Scroll".

## xte
_xte_ is a handy little command-line tool that can emulate key presses and mouse clicks. In Debian/Ubuntu, it can be found in the xautomation package.

You can use _xte_ to execute a squence of key presses. For example, the following command will go 5 tabs to the right in firefox:
	
	xte 'keydown Control_L'; for i in `seq 1 5`; do xte 'key Page_Down'; done; xte 'keyup Control_L'
	
Here's a command that will enter today's date into a document:
	
	xte "str `date +%D`"
	

## wmctrl
_wmctrl_ allows you to do various window manager actions from the command line.  A useful feature is "wmctrl -a <str>", which will switch to a window containing the string <str> in the title.  For example, to activate a firefox window, and start firefox if there is no such window, use the following command:
	
	wmctrl -a Firefox || firefox
	

## GUI testing frameworks
GUI testing frameworks such as [dogtail](http://people.redhat.com/zcerza/dogtail/) and [LDTP](http://ldtp.freedesktop.org/wiki/) offer much better control over applications than what is possible by emitting key strokes, but they're also more difficult to use and they also require assistive technologies to be enabled. If you have a good example of what is possible with those tools, please add it here.

# Wacom Tablet PCs

## TPCButtons
Turn TPCButtons off, so that you can click the button on your pen without having to tap the pen at the same time. This can be done using the command
	
	xsetwacom set stylus TPCButton 0
	
or, permanently, by adding the line
	
	        Option          "TPCButton"     "off"
	
to the *last* "InputDevice" section pertaining to your tablet. Note that in order for this to be persistent through suspend or switching VTs, you'll need the latest linuxwacom driver from git ([relevant commit](http://git.debian.org/?p=collab-maint/linux-wacom.git;a=commit;h=4a3811a1c3120b1a50bc2fd6848ea18470ea6465)). See also [FeatureAdvancedGestures].

## Don't use Rotate Scripts
There are tons of applications out there that allow you to change screen resolution and rotation. However, as a tablet user you will also need tell the  wacom driver to rotate pen input. So people ditch these applications and replace them by something much worse: Custom-made rotate scripts that must be used if you want to your pen to be usable after rotate - very annoying and cumbersome to set up.

But Xrandr is perfectly capable of notifying applications of screen changes, so a much better solution is to have a tiny daemon that performs the necessary actions whenever the screen is rotated. The wacomrotate daemon is an example of this: It will both rotate the pen and, if you're running gnome, adjust the orientation of subpixel smoothing, which will make fonts less blurry.  You can find the source at [github](http://github.com/thjaeger/wacomrotate/tree/master), and packages for jaunty and karmic in one of my [PPAs](https://launchpad.net/~thjaeger/+archive/tabletpc)

# Useful Tablet PC apps

These applications have nothing to do with easystroke per se, but I've found them highly valuable, so I'd like to spread the word.
  * [xournal](http://xournal.sourceforge.net/) is a great note-taking application. It supports PDF annotation and can take advantage of the full digitizer resolution.
  * [cellwriter](http://risujin.org/cellwriter) is a handwriting input panel/onscreen keyboard with a very pleasant and intuitive user interface.
  * [Grab and Drag](http://grabanddrag.mozdev.org/) is a firefox add-on that makes scrolling with a pen a breeze. It also works for thunderbird, by the way.
  * Many Tablet PCs come with high-resolution displays (145 dpi in case of the x61t and the LE1700) that can cause strain when used at the default font settings. The [NoSquint](http://urandom.ca/nosquint/) firefox extension allows you to set a default zoom setting for all web pages and separate settings for specific sites.