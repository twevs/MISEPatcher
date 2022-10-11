# MISEPatcher

## Description

Windows executable that patches the _Secret of Monkey Island: Special Edition_ process to prevent it from pausing and minimizing when it is in fullscreen mode and detects a click outside of its window.

## Instructions

Run the executable at any point after the game has reached the main menu. (The game executable being obfuscated by Steam DRM makes file patching inconvenient, but since the D3D9 module is also modified, runtime patching at least has the advantage of not affecting the behaviour of other D3D9 games in possibly unintended ways.)

## Caveats

I have only tested this on a 64-bit Windows 10 machine with the game launched via Steam and cannot guarantee that it will work on other setups.

## Known issues

- After clicking outside of the game window, one has to click in the window again to restore mouse tracking.
- Removes the Windows Night Light overlay from the game.

## Thanks

To [Matti](https://github.com/Mattiwatti) and Sir Kane on the [Reverse Engineering Discord server](https://discord.gg/weKN5wb) for pointing me towards DirectX in my investigation of this behaviour.