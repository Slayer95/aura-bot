Aura
====

Overview
--------

[Aura][1] is a modern cross-platform hosting bot for Warcraft III The Frozen Throne Ⓡ.
It's based on [GHost++][2] by Trevor Hogan. After several overhauls across 
the entire code base, it now has excellent performance, and extensive networking, 
configuration, and input capabilities, making it very easy to use not only by players, 
but also bot owners, even behind the most strict [NATs][3].

Aura runs on little-endian Linux (32-bit and 64-bit), Windows (32-bit and 64-bit), 
and OS X (64-bit Intel CPU) machines.

Getting started
---------------

First, get the Aura executable file. They are already provided for Windows 32-bit and 64-bit. 
For Linux, or OS X, you will need to build it yourself. See [BUILDING.md][4] for instructions.

To run the executable, you will need to use a command line. In Windows, open Command Prompt 
(type cmd into the Start menu and it should be the first result). In Mac OS X, open Terminal 
(it's in Utilities).

Type this into the command line:

```
cd LOCATION
```

Replace ``LOCATION`` with the location Aura is in (ending up with, for instance, ``cd "~/Downloads/aura-bot"`` 
or ``cd "C:\Users\Jon\Downloads\aura-bot\")``.

This will set your command line's location to Aura's folder. You'll have to do this each time you open 
a command line to run Aura.

Copy ``aura-example.cfg`` into ``config.ini``, and edit as you please. Aura is capable of autoconfiguration to some
extent, but you will need to provide it with ``bot.maps_path`` to let it find your maps folder. This should 
be in the same format as ``LOCATION`` above. See [CONFIG.md][5] for documentation regarding config options.

Now, to test your setup, run the command:

```
aura wormwar.cfg "my first hosted game" --filetype config 
```

Open your game client, and go to **Local Area Network**. You should see your Worm War game hosted there. 
Join it and type to the chat ``!owner``. This is a bot command. You may use them from many channels, 
including Battle.net, PvPGN public chats, whispers, IRC networks, and your system command line.

You may run ``!unhost`` to cancel this game, and proceed with the next step.

Run the commmand:

```
aura "Lost Temple" "testing config"
```

Open your game client, and go to ``Local Area Network``. If you have properly configured ``<bot.maps_path>``, 
you should see your Lost Temple game hosted there.

Join it, and take ownership by running the command ``!owner``. Afterwards, diagnostic your network 
connectivity with ``!checknetwork *``. See [NETWORKING.md][6] for additional information and troubleshooting.

Once you are satisfied, you may add fake players or computers (``!fp``, or ``!fill``) before starting 
the game (it cannot start with you alone.) Games may be started with ``!start`` or a few alternative 
commands, which you may find at [COMMANDS.md][7].

In both CLI examples so far, we have seen how to:
a. Host a known game from a config file at ``<bot.map_configs_path>``.
b. Host a known game from a map file at ``<bot.maps_path>``.

There are more CLI features available at [CLI.md][8].

However, you likely want to start Aura and let authorized people host any number of games. 
For that, setup your ``config.ini`` to connect to one or more realms, or to an IRC network. Then, 
just run the command:

```
aura
```

Now, you may join the same server as your bot, and send commands to it through public chat or whispers.

Features
---------

Find a non-extensive list of Aura features at [FEATURES.md][9].

Credits
-------

* Leonardo Julca -- [Aura v2.0][1] author
* Josko Nikolic -- [Aura v1.32][9] author
* Trevor "Varlock" Hogan -- GHost++ author
* MrJag -- [Ghost][10] author

Contributing
------------

This project is open-source, and hosted at [Gitlab][1]. You will need [Git][11] in 
order to contribute to this and other open-source projects. Then proceed with the 
following:

1. Fork the [aura-bot][1] repository.
2. Create a branch (`git checkout -b my_aura`)
3. Commit your changes (`git commit -am "Fixed a crash when using GProxy++"`)
4. Push to the branch (`git push origin my_aura`)
5. Create an [Issue][12] with a link to your branch or make a [Pull Request][13]

[1]: https://gitlab.com/ivojulca/aura-bot
[2]: https://github.com/uakfdotb/ghostpp
[3]: https://en.wikipedia.org/wiki/Network_address_translation
[4]: https://gitlab.com/ivojulca/aura-bot/BUILDING.md
[5]: https://gitlab.com/ivojulca/aura-bot/CONFIG.md
[6]: https://gitlab.com/ivojulca/aura-bot/NETWORKING.md
[7]: https://gitlab.com/ivojulca/aura-bot/COMMANDS.md
[8]: https://gitlab.com/ivojulca/aura-bot/CLI.md
[9]: https://github.com/Josko/aura-bot
[10]: https://github.com/MrJag/ghost
[11]: https://git-scm.com/book/en/v2/Getting-Started-Installing-Git
[12]: https://gitlab.com/ivojulca/aura-bot/-/issues
[13]: https://gitlab.com/ivojulca/aura-bot/-/pulls
