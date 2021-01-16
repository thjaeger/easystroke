Back to the main [Documentation](../Documentation) page.

Advanced gestures are gestures where you click an additional button while already holding down the trigger button.

The feature can be used in two different ways, depending on the value of the option "Ignore strokes leading up to advanced gestures". If the option is set, the stroke that has been performed prior to triggering the advanced gesture will be discarded, so you essentially get as many advanced gestures as your mouse has buttons, minus the one that is used to trigger gestures. If the option is not checked, advanced gestures are also distinguished by their stroke. Note that until you release the gesture trigger button, the same stroke will be used for subsequent actions.

# Preview pictures

|| [[Image(MiscWikiFiles:advanced-button.png, align=center)]] || This is the picture of an advanced gesture where both buttons are pressed without moving the mouse in between.
|| [[Image(MiscWikiFiles:advanced-stroke.png, align=center)]] || This is the symbol for a stroke followed by a click of an additional button. The "Ignore..." option needs to be disabled in order to use this type of gesture.
|| [[Image(MiscWikiFiles:advanced-click.png, align=center)]] || This one is just a single click of the gesture trigger button.

# Examples

## Emulate a right-click on a Tablet PC pen
A big shortcoming of most Tablet PC pens is that they only two "buttons": The tip of the pen and one button on the side. You can set up Easy Gesture to treat pressing the side button, followed by a regular click as a right click. Just open the "Actions" tab and add a new action. Choose "Button" as its type and then select the right button (Button3). Finally you need to record the stroke, which is just an unreleased middle-click followed by a right-click.

## Alt+Tab switching in Compiz
Setting up regular gestures to switch between windows by pressing Alt+Tab won't work very well: The problem is that this requires holding down the Alt key, but regular gestures have to release the key immediately. This is where advanced gestures come in: Now the modifiers associated to the last action will be held down until the gesture is over, i.e. until you release the gesture trigger button. As an example, let's set up Easy Gesture to allow Alt+Tab switching by performing an "up"-stroke (without releasing the trigger button), followed by flicking the scroll wheel. Make sure that the "Ignore..." option described above is disabled. This time we need to add two actions, one for the up button (Button4) and one for the down (Button5). Set them up to be key actions, pressing Alt+Tab and Alt+Shift+Tab, and then record the two strokes. Not that you will have to perform the stroke for both mouse buttons, once followed by an up flick and once by a down flick, but when you're actually executing the action multiple times in a row, you only need to perform the stroke once.

# Preparation

If you want to use this feature on a Tablet PC, you should turn off the linuxwacom driver's TPCButtons option, so that just pressing the side switch will produce a middle click, see [TipsAndTricks](../TipsAndTricks) for details.