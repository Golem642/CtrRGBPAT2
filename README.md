# CtrRGBPAT2
This 3DS application allows the LED color to be changed. In order for this to work, Luma CFW (custom firmware) needs to be installed.
Thanks to [CPunch](https://github.com/CPunch/CtrRGBPATTY/) for the original project !

## Features
Customize the LED color and pattern for when you receive notifications! Why keep the default blue when you can have a cool-looking purple for example 😎
You can customize the LED for every type of notification (there are more than you think...)
Everything is simple and made so you cannot possibly screw something up (unless you really wanted to)

## Upcoming
- [x] ~~StreetPass and Friends notification support (+ two unknown/unused? notification types)~~
- [x] ~~"Low Battery" notifications support~~
- [x] ~~Full editor to allow you to create your own patterns~~
- [x] ~~Separate settings for Sleeping/Awake~~
- [x] ~~Preview in a lot of places~~
- [x] ~~Presets loading~~
- [ ] Save and restore feature
- [ ] NFC color receiving ?
- [x] ~~Toggle on/off~~
- [ ] Proper UI ?

# QnA
**Q : My console crashes on boot after the patch !**
 - A : Delete the files at `/luma/sysmodules/0004013000003502.ips` and `/luma/titles/0004013000003502/code.ips` and try again. Do not use the app, send a GitHub issue in this repo, and wait for the next release where there will be a fix.

**Q : When will \[insert feature\] will be available ?**
 - A : This project is mainly for fun and training, so I will implement features when I feel like it.

**Q : What does the two unknown patterns do ?**
 - A : I found out that the "Unknown" patterns triggers when your console boots or exits sleep mode, it can make up for some pretty cool transitions for when you open the lid. As for the second friend notification type, it's probably for when someone plays a game you can join using the friend list, still unsure though.

# Building
After some research I determined that this is made for devkitpro for Windows. I don't know how CPunch managed to make this work but I had to make a lot of changes. 

The compilation process is very weird but I didn't want to change anything to avoid breaking the currently "working" setup. 
The main program is in `build/main.cpp`, to compile it you can use the devkitPro Windows MSYS installation with `make` at the root of the project folder.
This should give you 4 types of file in the output folder: `3ds` (citra), `3dsx` (homebrew launcher), `cia` (install with FBI), `smdh` (info file for building).

I might modify the setup in the future to make things more consistent and working everywhere.

# Contributing
I know my code isn't the cleanest, but any contribution is appreciated ! Even if you don't code, you can also suggest ideas or report bugs in the issues and discussions tabs

<hr>

# Initial README contents :

## eclipse-3ds-template
This template is basically a fork of [TricksterGuy's 3ds-template](https://github.com/TricksterGuy/3ds-template) which is essentially a fork of two other templates, which I'm not going to ramble on about.

## Modifications:
1) Supports eclipse now.
2) Eclipse also has build targets.

## To set up in eclipse:
Open eclipse and import a project into the workspace. Select the root of the folder (or select the ZIP of this repository). You must have devkitPro installed to /opt/devkitPro and thus devkitARM installed to /opt/devkitPro/devkitARM.

Atop this, you also need buildtools.

As a result, all build targets and includes should be added. If you'd like auto-corrections on libraries other than ctrulib.

## To set up for elsewhere (or Windows.)
Modify the eclipse environment variables, C includes, and C++ includes to be where your install of devkitPro and CTRUlib is.
