# SuperTuxKart
[![Build Status](https://travis-ci.org/supertuxkart/stk-code.svg?branch=master)](https://travis-ci.org/supertuxkart/stk-code)
[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/supertuxkart/stk-code?svg=true&branch=master)](https://ci.appveyor.com/project/supertuxkart/stk-code)
[![#supertuxkart on the freenode IRC network](https://img.shields.io/badge/freenode-%23supertuxkart-brightgreen.svg)](https://webchat.freenode.net/?channels=supertuxkart)

SuperTuxKart is a free kart racing game. It focuses on fun and not on realistic kart physics. Instructions can be found on the in-game help page.

The SuperTuxKart homepage can be found at <https://supertuxkart.net/>. There is also our [FAQ](https://supertuxkart.net/FAQ) and information on how get in touch with the [community](https://supertuxkart.net/Community).

Latest release binaries can be found [here](https://sourceforge.net/projects/supertuxkart/files/SuperTuxKart/1.0/).

## Hardware Requirements
To run SuperTuxKart, make sure that your computer's specifications are equal or higher than the following specifications:

* A graphics card capable of 3D rendering - NVIDIA GeForce 8 series and newer (GeForce 8100 or newer), AMD/ATI Radeon HD 4000 series and newer, Intel HD Graphics 3000 and newer. OpenGL >= 3.3
* You should have a CPU that's running at 1 GHz or faster. 
* You'll need at least 512 MB of free VRAM (video memory).
* Minimum disk space: 800 MB 
* Ideally, you'll want a joystick with at least 6 buttons.

## License
The software is released under the GNU General Public License (GPL) which can be found in the file [`COPYING`](/COPYING) in the same directory as this file. Information about the licenses for the artwork is contained in `data/licenses`.

---

## 3D coordinates
A reminder for those who are looking at the code and 3D models:

SuperTuxKart: X right, Y up, Z forwards

Blender: X right, Y forwards, Z up

The export utilities  perform the needed transformation, so in Blender you just work with the XY plane as ground, and things will appear fine in STK (using XZ as ground in the code, obviously).

## Building from source

Building instructions can be found in [`INSTALL.md`](/INSTALL.md)


# STK Addon Server Modification

This modification allows to create servers that allow them to let in only users with certain number of addon tracks. You need to insert only one xml element `<addon-tracks-threshold value="YOUR_THRESHOLD" />` into your server config, for example, like this:

```
...
<official-karts-threshold value="1" />
<official-tracks-threshold value="0.7" />
<addon-tracks-threshold value="15" />
<min-start-game-players value="1" />
...
```

The code above will tell all users with less than 15 addons that their game data is incompatible, and will allow entry for other users.

The modification also applies to sqlite STK servers, it creates a field `addons_count` which stores the number of addons for a connected player. Be careful then when applying this code to a server with existing database, you may need to copy all data from previous database to a new one and fill `addons_count` with a not-null value.

# Other STK information

All info not regarding this modification can be found in official STK repo: github.com/supertuxkart/stk-code/
