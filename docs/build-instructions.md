# Overview

Easy Gesture is split into two parts - the GUI and the service. 

The service runs in the background, watching for gestures and running commands. It's configured through ~/.easy-gesture/config.toml and ~/.easy-gesture/gestures.yaml.
The GUI provides an easy way to change config.toml or gestures.yaml (but it's not necessary to use the GUI).

# Debian/Ubuntu
	
	sudo apt-get install g++ libboost-serialization-dev libgtkmm-3.0-dev libxtst-dev libdbus-glib-1-dev intltool xserver-xorg-dev
   sudo apt-get install -y python3-venv python3-wheel python3-dev
   sudo apt-get install -y libgirepository1.0-dev build-essential
   sudo apt-get install -y libbz2-dev libreadline-dev libssl-dev zlib1g-dev libsqlite3-dev wget
   sudo apt-get install -y  curl llvm libncurses5-dev libncursesw5-dev xz-utils tk-dev libcairo2-dev
   pip3 install pycairo PyGObject

# Releases

- These patches were applied on top of [git master](https://github.com/thjaeger/easystroke/):

   - [fix build failed in libsignc++ version 2.5.1 or newer](https://github.com/thjaeger/easystroke/pull/9/commits/22b28d25bb696e37e73b4bc641439b3db9f564ed) build fix
   - [Remove abs(float) function that clashes with std::abs(float)](https://github.com/thjaeger/easystroke/pull/8/commits/9e2c32390c5c253aade3bb703e51841748d2c37e) build fix
   - [fixed recurring crash when trying to render 0x0 tray icon](https://github.com/thjaeger/easystroke/pull/10/commits/140b9cae66ba874bf0994eea71210baf417a136e) bug fix
   - [dont-ignore-xshape-when-saving.patch](https://aur.archlinux.org/cgit/aur.git/tree/dont-ignore-xshape-when-saving.patch?h=easystroke-git) bug fix, fixes [Changing 'method to show gestures' to Xshape does not persist](https://bugs.launchpad.net/ubuntu/+source/easystroke/+bug/1728746)
   - [switch from fork to g_spawn_async](https://github.com/thjaeger/easystroke/pull/6/commits/0e60f1630fc6267fcaf287afef3f8c5eaafd3dd9) bug fix `This fixes a serious bug that can lead to system instability. Without this patch, if a 'Command' action is commonly used, it will lead to so many zombie processes that the OS will be unable to launch additional processes.`
   - [add-toggle-option.patch](https://aur.archlinux.org/cgit/aur.git/tree/add-toggle-option.patch?h=easystroke-git) new feature: adds a actvation/deactivation toggle when right-clicking the tray icon``
   - [easystroke-0.6.0-gnome3-fix-desktop-file](https://src.fedoraproject.org/cgit/rpms/easystroke.git/commit/?id=4d59e8e1e849a09887c4588c84a1e1e02c350949) cosmetic fix


A list of all available Patches can be found [here](https://github.com/thjaeger/easystroke/pull/10#issuecomment-444132355) for now.
