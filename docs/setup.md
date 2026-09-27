---
layout: default
title: Setup Guide
---
# Setup Guide
## Table of Contents
1. [Overview](#overview)
2. [Selecting Menu Icons](#selecting-menu-icons)
3. [Maintaining Contrast](#maintaining-contrast)
4. [Launching a Web Browser](#launching-a-web-browser)
5. [Watching YouTube](#watching-youtube)
6. [Directly Launching Steam Games](#directly-launching-steam-games)

## Overview
This page contains tips for setting up StreamFlex, and HTPCs in general. The recommendations herein are broadly applicable to all platforms supported by StreamFlex. Additionally, see the platform setup guides for platform-specfic advice:
- [Windows Setup Guide](https://bilbospocketses.github.io/streamflex/setup_windows)
- [Linux Setup Guide](https://bilbospocketses.github.io/streamflex/setup_linux)

Make that you are generally familiar with the [configuration options](https://bilbospocketses.github.io/streamflex/configuration) as well.

## Selecting Menu Icons
StreamFlex ships an [icon library](icons): the app icons of popular streaming and media services, and generic icons for system actions, kinds of media and more. Use one by writing its name in place of an icon path, for example `Entry1=Netflix;netflix;...`.

To use your own icons, transparency is essential, so avoid JPEG, which does not support it; use PNG, WebP or SVG instead.

Icons are scaled to the size of their button, which the menu's grid decides (see [Layout](configuration.md#layout)), so the same icon may be drawn at different sizes in different menus. A PNG or WebP icon scales down best from a large original, 256x256 or more. An SVG icon is drawn at the button's exact size, so it stays sharp at any size; where an SVG version of an icon exists, it is the best choice.

## Maintaining Contrast
When using an image as the background, it is often difficult to read the text that is displayed on top. This is particularly true if the image is a photograph and the text is white. StreamFlex has several features that will improve the contrast between the background and the objects on top.

The background overlay feature draws a solid color, typically black, over the background. This will darken the background to improve the contrast ratio. The user can adjust how much to darken the background with the `OverlayOpacity` setting.

Text shadows will give displayed text a textured, 3 dimensional appearance, which helps it stand out from the background.

The highlight and scroll indicators each have an outline setting. The user can choose how thick and which color the outline should be, to improve the contrast with the background.

## Launching a Web Browser
My recommended web browser for HTPC use is Chrome/Chromium. This browser has many command line launch options which make it more flexible to configure than Firefox and its derivatives. Some launch options that have particular relevance to HTPC use:
- `--start-fullscreen`: This starts the browser in a fullscreen mode. However, do note that the address bar will be hidden, so make sure to include the URL of the website you want to launch as an argument.
- `--force-device-scale-factor=n`: This can be used to make web pages rendered larger for viewing from a distance. For example, try, 1.1 or 1.2 as `n`.
- `--user-agent`: Sets a custom HTML user-agent string. This is necessary for [watching YouTube](#watching-youtube).

## Watching YouTube
There is currently no desktop application for YouTube. However, there is a TV-friendly web interface located at [youtube.com/tv](https://www.youtube.com/tv) that is intended for use by Smart TVs . Google recently blocked access to this interface for desktop web browsers, but the block can be easily circumvented by spoofing the user-agent string of a Smart TV. A list of valid Smart TV user-agent strings is easily found online by search engine. The following example menu entries will launch an app-like YouTube experience in a browser:

**Windows:**
```ini
Entry=YouTube;C:\icons\youtube.png;"C:\Program Files\Google\Chrome\Application\chrome.exe" --start-fullscreen --user-agent="Mozilla/5.0 (Linux; Tizen 2.3; SmartHub; SMART-TV; SmartTV; U; Maple2012) AppleWebKit/538.1+ (KHTML, like Gecko) TV Safari/538.1+" youtube.com/tv
```

**Linux:**
```ini
Entry=YouTube;/path/to/icons/youtube.png;chromium --start-fullscreen --user-agent="Mozilla/5.0 (Linux; Tizen 2.3; SmartHub; SMART-TV; SmartTV; U; Maple2012) AppleWebKit/538.1+ (KHTML, like Gecko) TV Safari/538.1+" youtube.com/tv
```

This method is far superior to other HTPC YouTube options, such as Kodi's YouTube add-on. You can install a browser extension such as [uBlock Origin](https://ublockorigin.com/) to prevent ads from being shown before videos.

The web interface also supports casting videos from the YouTube app on your smartphone to your TV. You can pair your phone in the settings. You can also sign into your YouTube account in the settings if you wish.

### Exiting
The one caveat to this method is that the exit button in the menu doesn't work. As such, you will need to provide an alternative method to close the web browser after you've finished watching so you can return back to the launcher. For Windows users, the most straightforward solution is to configure an [exit hotkey](https://bilbospocketses.github.io/streamflex/configuration#exit-hotkey-windows-only) on your remote. Linux users should set up a hotkey with their DE/WM to close the active window.

## Directly Launching Steam Games
Steam users may desire to launch their most frequently played games directly from StreamFlex to avoid having to navigate through the Steam client UI first. Valve provides a [protocol](https://developer.valvesoftware.com/wiki/Steam_browser_protocol) to directly launch games, among other actions. To do so, pass `steam://run/<id>` as an argument to Steam, where `<id>` is replaced by the id of the game you want to watch. You can find the id of a game by searching [steamdb](https://steamdb.info/). For example, the id of Portal 2 is 620. You would structure your menu entry to launch Portal 2 like so:

**Windows:**
```ini
Entry=Portal 2;C:\icons\portal_2.png;"C:\Program Files (x86)\Steam\steam.exe" steam://run/620
```

**Linux:**
```ini
Entry=Portal 2;/path/to/icons/portal_2.png;steam steam://run/620
```

Make sure you have autologin configured in Steam, otherwise you will be prompted for your password before the game launches.
