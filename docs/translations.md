There are two ways to contribute translations to easystroke:

# Launchpad

I've set up a launchpad project so that you can edit the translation strings using a simple web interface:

https://translations.launchpad.net/easystroke/trunk/+pots/easystroke

Before each release, I will pull in the latest translations from launchpad.

To test your translations (assuming you're running a recent enough version of easystroke), download the .mo file from launchpad and copy it to /usr/share/locale/??/LC_MESSAGES/easystroke.mo, where ?? represents the language code.

# Manual

The downside to the launchpad approach is that you don't immediately see the effects of your translations. You can also directly edit translations in the source: First clone the easystroke repository (as explained in BuildInstructions). If there already is a translation file for your language (po/??.po), you can either edit the file by hand or use an po file editor such as gtranslator. Otherwise you need to base the file off the messages.pot file that can be built by typing 'make translate' (or just ask me to create a .po file for your language). To test your translations, you need to compile the po file ('make compile-translations') and then execute easystroke from the root directory of the repository.

If you go this route, please check whether launchpad has updated translations first. If that's the case, either download the .po file directly from launchpad and place it in the po directory or ask me to import it into git.

# Strings that need additional explanation

## %1 "%2" (containing %3 %4) is about to be deleted
%1 is your translation of "The application" or "The group"

%2 is the name of the application or group

%3 is the number of actions

%4 is your translation of "action"/"actions".

# Completeness

Note that I will only import translations into git and include them into official releases if they're reasonably complete.  I believe that mixing two languages is more confusing to the user than having the whole interface in a foreign language.  In Launchpad, you can tell whether your new translations have been exported by their color: green means the translations are in git, light blue and purple mean they are only in Launchpad.