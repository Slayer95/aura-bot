Command Line Interface
======================

DISCLAIMER: A significant part of this document conveys project intent, and not
necessarily the current status of the code base. I am just writing this up now,
so I don't get lazy later. Proceed with caution.

TODO(IceSandslash)

# Overview

[Aura][1]'s CLI provides a set of flags and parameters to configure and control the bot's behavior
when hosting games, or to host games themselves.

Below is a detailed explanation of each flag and parameter along with usage examples.

# Positional arguments

## `<MAP> <NAME>`
Specifies the map and the name for the hosted game. 

- `<MAP>`: The name or identifier of the map to be hosted.
- `<NAME>`: The desired name for the hosted game session.

## `<CONFIG> <NAME>`
Specifies the configuration and the name for the hosted game.

- `<CONFIG>`: The configuration settings for the game.
- `<NAME>`: The desired name for the hosted game session.

# Flags

\`--about\`

Displays the information of the host bot, including its version.

\`--help\`

Displays usage information and a list of available flags and parameters.

\`--lan\`

This flag indicates that any hosted games should be available in the 
"Local Area Network" game menu. When this flag is used, hosted games 
will be available to players on the same network, as well as players 
sharing the same VPNs or the IPv6 TCP tunneling mechanism. 
See [NETWORKING.md][2] for more information.

This flag is enabled by default.

\`--no-lan\`

Conversely, this flag indicates that the game should NOT be available in the 
"Local Area Network" game menu. When this flag is used, hosted games 
will NOT be available to players on the same network, nor to players 
sharing the same VPNs or the IPv6 TCP tunneling mechanism. 
See [NETWORKING.md][2] for more information.

\`--bnet\`

Sets all Battle.net/PvPGN realms registered in \`aura.cfg\` to enabled. This flag overrides 
any ``realm_N.enabled = no`` entries in the configuration file. This forces hosted games 
to be playable over the internet through Blizzard's Battle.net platform, as well as 
through alternative PvPGN servers.

\`--no-bnet\`

Sets all Battle.net/PvPGN realms registered in \`aura.cfg\` to disabled. This prevents Aura from 
connecting to them. If this flag is enabled, games cannot be played over the internet through 
Blizzard's Battle.net platform, nor through alternative PvPGN servers.

\`--exit\`

Enables the option to automatically exit the bot after hosting all games queued through the CLI. 
When this option is enabled, the bot will terminate itself once the hosting process is completed. 
When any games are created from the CLI, this option is automatically enabled by default.

\`--no-exit\`

Disables the option to automatically exit the bot after hosting all games queued through the CLI. 
When this option is enabled, the bot will terminate itself once the hosting process is completed.

\`--cache\`

Enables caching of map metadata for faster loading times and improved performance during gameplay. 
When caching is enabled, certain map data is stored locally to reduce loading times for subsequent 
sessions.

This option is enabled by default when no games are hosted from the CLI.

\`--no-cache\`

Disables caching of map metadata. When caching is disabled, game data is not stored locally, and 
each session may experience longer loading times as a result. This option is enabled by default 
when hosting games from the CLI.

# Parameters

\`--w3version <NUMBER>\`

Specifies the version that is to be used when hosting Warcraft 3 games. There is no cross-version 
compatibility. This parameter allows the host bot to switch versions on the spot.

\`--w3path <DIRECTORY>\` 

Specifies the directory where the Warcraft 3 game files are located. This parameter allows the host bot 
to identify and fetch files from alternative Warcraft 3 game installations on the spot.

\`--mapdir <DIRECTORY>\`

Specifies the directory where Warcraft 3 maps are stored. This parameter allows the host bot to locate  
and load maps for hosting games.

\`--cfgdir <DIRECTORY>\`

Specifies the directory where the metadata of Warcraft 3 maps is stored. Aura only needs the metadata 
in order to host games, even after the actual map files are gone. This parameter allows the host bot 
to locate the metadata files, also referred to as "map config" files, or \`mapcfg\` for short.

\`--udp <strict|lax|free>\`

# Flags for CLI games

\`--stdpaths\`

When enabled, this flag instructs the host bot to utilize standard paths for directories and files 
entered from the CLI. Notably, it ensures that map and configuration file paths are interpreted 
exactly as entered, thus disabling fuzzy-matching. It also makes them relative to the current working 
directory, instead of the root path of the bot.

However, it's important to note that this flag removes protection against [arbitrary directory traversal][3]. 
Therefore, it should only be used for paths that have been thoroughly validated.

\`--random-races

This flag enables randomization of player races in the hosted game. When this flag is used, each player's 
race will be randomly assigned at the start of the game session. This adds an element of unpredictability 
and variety to the game, as players may need to adapt their strategies based on their randomly assigned race.

\`--random-heroes

This flag enables randomization of player heroes in the hosted game. With this flag, each player's hero will 
be randomly selected at the beginning of the game session. Randomizing heroes adds excitement and diversity 
to the gameplay, as players must adapt their tactics based on the strengths and abilities of their randomly 
assigned hero.

# Parameters for CLI games

\`--filetype <TYPE>\`

Specifies the type of file being referenced. This parameter helps the host bot determine how to handle 
the specified files, whether they are maps or configuration files.

**Options:**

- map: Indicates that the specified file is a Warcraft 3 map.
- config: Indicates that the specified file is a map config/metadata file.
- any: Indicates that the specified file may be either a Warcraft 3 map or config file. This is the default.

\`--exclude <SERVER>\`

This makes hosted games invisible in the specified server. The server is to be specified through their 
unique `Input ID`, which corresponds to ``realm_N.input_id`` in \`aura.cfg\`. Note that this value may be 
missing, and thus defaults to ``realm_N.unique_name`` (which defaults to \`realm_N.host_name\`).

\`--mirror <IP:PORT#ID>\`

This option sets Aura to use game mirroring mode. In this mode, the bot won't host games by itself, but 
instead repost a game hosted elsewhere to connected Battle.net/PvPGN realms. The actual host is identified 
by their IPv4 address and PORT. The game ID, also known as "host counter", must also be provided.

Aura will remain in game mirroring mode until the process finishes.

\`--observers <OBSERVER>\`

This parameter sets the observer mode for the hosted game. Observer mode determines how spectators are 
allowed to observe the game. Here are the available options:

- none: No observers allowed. Spectators cannot join the game.
- full observers: Allows spectators to exclusively observe the game, and chat among themselves.
- referees: Allows spectators to observe the game, and communicate with the players, either in public chat, or private chat.
- observers on defeat: Players that lose the game are allowed to remain in the game, as spectators.

\`--visibility <VISIBILITY>\`

This parameter sets the visibility mode for the hosted game, determining how much of the map is visible 
to players and observers. Here are the available options:

- default: Standard visibility mode where players can only see areas of the map revealed by their units and structures.
- hide terrain: Hides terrain from players and observers, making the entire map appear as black fog of war.
- map explored: Reveals the entire map to players, allowing them to see all terrain but not enemy units or structures unless they have vision of them.
- always visible: Grants players and observers full visibility of the entire map, including enemy units and structures, without the need for vision.

\`--owner <USER@SERVER>\`

This parameter specifies the owner of the hosted game. The owner is typically the user who has 
administrative control over the game session. Here's the format for specifying the owner:

- <USER>: The username of the owner.
- <SERVER>: The server the user is registered in.

# Flags for CLI commands

\`--exec-broadcast\`

This flag enables broadcasting the command execution to all users or players involved in the game 
session. When activated, the command specified by --exec will be broadcasted to all participants, 
ensuring transparency and visibility of the executed action.

By using this flag, users or players will be informed about the command being executed, fostering 
a collaborative and informed environment during the game session.

# Parameters for CLI commands

\`--exec <COMMAND>\`

This parameter allows the execution of a specific command after the bot start up, and the CLI game (if any) 
has been created. Here's how it works:

- <COMMAND>: The command to be executed. The command token is not to be included (e.g. ! or .)

\`--exec-as <USER@SERVER>\`

This parameter specifies the user and server on which the command specified by --exec should be executed. 
It allows the execution of commands as a specific user on a particular server. Here's the format:

- <USER>: The username to whom the command is attributed.
- <SERVER>: The server the user is registered in.

\`--exec-auth <AUTH>\`

This parameter sets the authentication mode for executing commands specified by --exec.
The available authentication modes are:

- spoofed: Treats the user as unverified by their respective server.
- verified: Treats the user as verified by their respective server.
- admin: Treats the user as an admin of their server.
- rootadmin: Treats the user as a root admin of their server.
- sudo: Treats the user as a bot sudoer. These are the highest privileges.

\`--exec-scope <SCOPE>\`

This parameter determines where the command specified by --exec will be run.
There are four ways to specify the scope:

- none: The command runs without any specific scope. This is the default behavior.
- lobby: The command runs in the currently hosted game lobby.
- server: The command runs in the user's realm or server.
- game#<GAME>: The command runs in the game with the specified ID.

Choose the appropriate scope based on where you want the command to be executed.


[1]: https://gitlab.com/ivojulca/aura-bot
[2]: https://gitlab.com/ivojulca/aura-bot/NETWORKING.md
[3]: https://owasp.org/www-community/attacks/Path_Traversal
