Features
============

[Aura][1] has a revamped core, and has had features not only added, but also removed.

Removed features from [GHost++][2]:
* No MySQL support
* No autohost
* No admin game
* No language.cfg
* No W3MMD support
* No replay saving
* No save/load games
* No BNLS support
* No boost required

Other changes:
* Uses C++17
* Single-threaded
* Has a Windows 64-bit build
* Uses SQLite and a different database organization.
* Tested on OS X (see [Building][2] -> OS X](#os-x) for detailed requirements)
* Updated libraries: StormLib, SQLite, zlib
* Connects to and can be controlled via IRC
* Using aggressive optimizations
* Up to 11 fakeplayers can be added.
* Uses DotA stats automagically on maps with 'DotA' in the filename
* Auto spoofcheck in private games on PvPGNs
* More commands added either ingame or bnet
* Checked with various tools such as clang-analyzer and cppcheck

[1]: https://gitlab.com/ivojulca/aura-bot
[2]: https://github.com/uakfdotb/ghostpp
[3]: https://gitlab.com/ivojulca/aura-bot/BUILDING.md
