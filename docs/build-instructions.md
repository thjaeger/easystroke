Easystroke is written in C++ and uses gtkmm for its GUI, but it also depends on the availability of several X11 extensions (Xtst, Xrandr and Xinput) and on the boost::serialization library. Installation of the libraries is distribution-specific. Please feel free to add instructions for your distro of choice.

# Debian/Ubuntu
	
	sudo apt-get install g++ libboost-serialization-dev libgtkmm-3.0-dev libxtst-dev libdbus-glib-1-dev intltool xserver-xorg-dev

or

	
	sudo apt-get build-dep easystroke
	
NOTE: Easystroke 5.x and below need libgtkmm-2.4-dev to build, not 3.0.

# Fedora
	
	yum install gcc-c++ gtkmm30-devel dbus-glib-devel boost-devel libXtst-devel intltool
	
NOTE: You might have to install 'xorg-x11-server-devel' for easystroke to compile probably (I had to).

# OpenSUSE
	
	zypper in make gcc gcc-c++ gtkmm2-devel glib2-devel dbus-1-glib-devel boost-devel intltool
	
NOTE: You might have to install 'xorg-x11-server-sdk' to successfully build on 12.X.
# Pardus
	
	pisi it make gcc  gtkmm boost-devel intltool
	
For Pardus, you also have to add -lXi to the LIBS row in the Makefile in order to get the app to compile.

# Releases
~~[Sourceforge download page](http://sourceforge.net/project/showfiles.php?group_id=229797)~~

NOTE: As for now, easystroke is in an unmaintained state. 

- The original author is not available. 
- The last release was on 23. March 2013, so ~6 Years ago.
- Even the master tree is not build-able any more without patches from unmerged pull requests. 
- As we do not have the credentials for this repository and the sourceforge page, an alternative solution has to be found. 
- Many distributions still build their packages from 0.6.0 release, which causes numerous problems. 
  - A list of the commits to git master since release can be found [here](https://github.com/thjaeger/easystroke/pull/10#issuecomment-444132355)
  - On Ubuntu's bug tracker for example, there are numerous bug entries where fixes are available, but they are not applied there.

**The steady flow of pull requests nevertheless shows the interest in this unique program**

- Preliminary binary bugfix-release of 2018/12/05: [AppImage](http://openartisthq.org/easystroke/easystroke-0.6.0-1ubuntu12-x86_64.AppImage) (built on Ubuntu 18.04; easystroke should run directly by making it executable and double clicking it.)

- Preliminary Debian package release with all patches applied:
[easystroke_0.6.0-0ubuntu8_amd64.deb](http://openartisthq.org/easystroke/easystroke_0.6.0-0ubuntu8_amd64.deb) (built on Ubuntu 18.04)

- Preliminary source-code bugfix-release of 2018/12/05:  [patched-easystroke-master.tar.bz2](http://openartisthq.org/easystroke/patched-easystroke-master.tar.bz2) (already patched, patches included for reference)

   - Short instructions for compiling (after you installed the build-dependencies for your distribution as stated above) : 

   ```
   wget http://openartisthq.org/easystroke/patched-easystroke-master.tar.bz2
   tar xvjf patched-easystroke-master.tar.bz2
   cd patched-easystroke-master/easystroke
   make
   ```

  then run it with
   `./easystroke`

  or install it to /usr/local with
   `sudo make install` 

  (in case your distribution does not have sudo installed, it's obviously just "make install"





- These patches were applied on top of [git master](https://github.com/thjaeger/easystroke/):

   - [fix build failed in libsignc++ version 2.5.1 or newer](https://github.com/thjaeger/easystroke/pull/9/commits/22b28d25bb696e37e73b4bc641439b3db9f564ed) build fix
   - [Remove abs(float) function that clashes with std::abs(float)](https://github.com/thjaeger/easystroke/pull/8/commits/9e2c32390c5c253aade3bb703e51841748d2c37e) build fix
   - [fixed recurring crash when trying to render 0x0 tray icon](https://github.com/thjaeger/easystroke/pull/10/commits/140b9cae66ba874bf0994eea71210baf417a136e) bug fix
   - [dont-ignore-xshape-when-saving.patch](https://aur.archlinux.org/cgit/aur.git/tree/dont-ignore-xshape-when-saving.patch?h=easystroke-git) bug fix, fixes [Changing 'method to show gestures' to Xshape does not persist](https://bugs.launchpad.net/ubuntu/+source/easystroke/+bug/1728746)
   - [switch from fork to g_spawn_async](https://github.com/thjaeger/easystroke/pull/6/commits/0e60f1630fc6267fcaf287afef3f8c5eaafd3dd9) bug fix `This fixes a serious bug that can lead to system instability. Without this patch, if a 'Command' action is commonly used, it will lead to so many zombie processes that the OS will be unable to launch additional processes.`
   - [add-toggle-option.patch](https://aur.archlinux.org/cgit/aur.git/tree/add-toggle-option.patch?h=easystroke-git) new feature: adds a actvation/deactivation toggle when right-clicking the tray icon``
   - [easystroke-0.6.0-gnome3-fix-desktop-file](https://src.fedoraproject.org/cgit/rpms/easystroke.git/commit/?id=4d59e8e1e849a09887c4588c84a1e1e02c350949) cosmetic fix


A list of all available Patches can be found [here](https://github.com/thjaeger/easystroke/pull/10#issuecomment-444132355) for now.

Detailed instructions to follow.

# Development Tree

As development is stalled, the following section is not relevant for now.

~~Most users will probably want to use a released version, so they can skip this section. However, there are a few reasons why someone would use the latest development version and you are absolutely encouraged to do so.~~

 * ~~You want to get a sneak preview of the awesome features that are planned for the next release.~~
 * ~~You want to help me out by trying to catch bugs before they make it into a release. This is greatly appreciated.~~
 * ~~You want to send me a patch.~~

~~Please always *keep a backup of your .easystroke configuration directory*, as the file format is often unstable during development and it is usually not possible to go back to the stable release once the configuration has been saved in a newer file format.~~

Note that some advanced features are only available if easystroke is started with the -e command line option.
______________________

Easystroke uses git for revision control. To fetch the development tree for the first time, type
	
	git clone git://github.com/thjaeger/easystroke.git
	
which will create a subdirectory 'easystroke' containing the sources. You can update to the latest tree anytime by changing into that directory and typing 'git pull'.

Now we're ready to build the program. Change into the easystroke directory and type 'make -j2'. This will create an executable file that you can either run directly or copy into your $PATH (easystroke does not require additional files to be installed), but of course you can also use 'make install' to install the program to /usr/local/bin. You can also create a small manpage using help2man by typing 'make man'.