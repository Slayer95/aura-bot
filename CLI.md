Command Line Interface
======================

# Overview

[Aura][1]'s CLI provides a set of flags and parameters to configure and control the bot's behavior
when hosting games, or to host games themselves.

Below is a detailed explanation of each flag and parameter along with usage examples.

# Positional arguments

## `<MAP> <NAME>`
Hosts a game with the given name, using a given map file.

- `<MAP>`: The name or identifier of the map to be hosted.
- `<NAME>`: The desired name for the hosted game session.

If ``MAP`` has no slashes, it's resolved relative to Aura's map dir by default.
When it has slashes, or ``--stdpaths`` is used, it's resolved relative to the current working directory (CWD).

## `<CONFIG> <NAME>`
Hosts a game with the given name, using a given map config/metadata file.

- `<CONFIG>`: The configuration settings for the game.
- `<NAME>`: The desired name for the hosted game session.

If ``CONFIG`` has no slashes, it's resolved relative to Aura's map config dir by default.
When it has slashes, or ``--stdpaths`` is used, it's resolved relative to the current working directory (CWD).

# Flags

## \`--about\`

Displays the information of Aura, including its version.

## \`--help\`

Displays usage information and a list of available flags and parameters.

## \`--verbose\`

Displays more detailed information when running CLI actions.

## \`--stdpaths\`

When enabled, this flag instructs Aura to utilize standard paths for directories and files 
entered from the CLI. That is, it makes them relative to the current working directory (CWD).

Notably, it also ensures that map and configuration file paths are interpreted exactly as entered, thus 
disabling fuzzy-matching. However, it's important to note that this flag removes protection against 
[arbitrary directory traversal][3]. Therefore, it should only be used for paths that have been thoroughly validated.

Additionally, the following CLI flags are affected by ``--stdpaths``:
- ``--config <FILE>``. When stdpaths is disabled and FILE has no slashes, it resolves relative to Aura's home dir.

This option is commutative.

## \`--init-system\`

When enabled, this flag instructs Aura to install itself to various relevant places.

- User PATH environment variable. This allows easily invoking Aura from anywhere in the CLI.
- Windows Explorer context menu. This allows easily hosting WC3 map files by right-clicking them.

This option is enabled by default when Aura is executed for the first time.

## \`--no-init-system\`

When enabled, this flag instructs Aura NOT to install itself to various relevant places. 
This can help in avoiding cluttering the system GUI and environment variables.

## \`--auto-port-forward\`

This flag enables Universal Plug and Play (UPnP) functionality within the game 
server. UPnP allows devices to discover and communicate with each other, 
facilitating automatic port forwarding on routers that support this protocol.

When the --auto-port-forward flag is enabled, the game server will attempt to 
automatically configure port forwarding on compatible routers using UPnP. This 
feature is particularly useful for hosting multiplayer game sessions, as it 
ensures that incoming connections from players outside the local network can 
reach the server without manual intervention to configure port forwarding on 
the router.

Note:
- UPnP support must be enabled on the router for this feature to work.
- Some routers may require user authentication for UPnP requests.

This flag is enabled by default.

## \`--no-auto-port-forward\`

Conversely, this flag flag is used to explicitly disable Universal Plug and Play 
(UPnP) functionality within the game server. When this flag is set, the game server 
will not attempt to automatically configure port forwarding using UPnP, even if 
UPnP is supported by the router.

This may be desirable in situations where manual port forwarding configurations are 
preferred, or if there are concerns about security risks associated with UPnP.

Disabling UPnP may require manual configuration of port forwarding rules on the router, 
which could be inconvenient for game hosts. Ensure that you can properly handle the 
implications of using this flag.

This option is equivalent to ``<net.port_forwarding.upnp.enabled = no>`` in \`config.ini\`

## \`--lan\`

This flag indicates that any hosted games should be available in the 
"Local Area Network" game menu. When this flag is used, hosted games 
will be available to players on the same network, as well as players 
sharing the same VPNs or the IPv6 TCP tunneling mechanism. 
See [NETWORKING.md][2] for more information.

This option is equivalent to ``<net.game_discovery.udp.enabled = yes>`` in \`config.ini\`

This flag is enabled by default.

## \`--no-lan\`

Conversely, this flag indicates that the game should NOT be available in the 
"Local Area Network" game menu. When this flag is used, hosted games 
will NOT be available to players on the same network, nor to players 
sharing the same VPNs or the IPv6 TCP tunneling mechanism. 
See [NETWORKING.md][2] for more information.

This option is equivalent to ``<net.game_discovery.udp.enabled = no>`` in \`config.ini\`

## \`--bnet\`

Sets all Battle.net/PvPGN realms registered in \`config.ini\` to enabled. This flag overrides 
any ``realm_N.enabled = no`` entries in the configuration file. This forces hosted games 
to be playable over the internet through Blizzard's Battle.net platform, as well as 
through alternative PvPGN servers.

This option is equivalent to ``<bot.toggle_every_realm = yes>`` in \`config.ini\`

## \`--no-bnet\`

Sets all Battle.net/PvPGN realms registered in \`config.ini\` to disabled. This prevents Aura from 
connecting to them. If this flag is enabled, games cannot be played over the internet through 
Blizzard's Battle.net platform, nor through alternative PvPGN servers.

This option is equivalent to ``<bot.toggle_every_realm = no>`` in \`config.ini\`

## \`--irc\`

When enabled, this flag instructs Aura to connect to the configured IRC server.

This option is equivalent to ``<irc.enabled = yes>`` in \`config.ini\`

## \`--no-irc\`

When enabled, this flag instructs Aura not to connect to IRC.

This option is equivalent to ``<irc.enabled = no>`` in \`config.ini\`

## \`--discord\`

When enabled, this flag instructs Aura to connect to Discord using the defined configuration.

This option is equivalent to ``<discord.enabled = yes>`` in \`config.ini\`

## \`--no-discord\`

When enabled, this flag instructs Aura not to connect to Discord.

This option is equivalent to ``<discord.enabled = no>`` in \`config.ini\`

## \`--exit\`

Enables the option to automatically exit the bot after hosting all games queued through the CLI. 
When this option is enabled, the bot will terminate itself once the hosting process is completed. 
When any games are created from the CLI, this option is automatically enabled by default.

## \`--no-exit\`

Disables the option to automatically exit the bot after hosting all games queued through the CLI. 
When this option is enabled, the bot will terminate itself once the hosting process is completed.

## \`--cache\`

Enables caching of map metadata for faster loading times and improved performance during gameplay. 
When caching is enabled, certain map data is stored locally to reduce loading times for subsequent 
sessions.

This option is enabled by default when no games are hosted from the CLI.

## \`--no-cache\`

Disables caching of map metadata. When caching is disabled, game data is not stored locally, and 
each session may experience longer loading times as a result. This option is enabled by default 
when hosting games from the CLI.

## `--extract-jass`

When you use this flag, Aura will automatically extract necessary files from your game 
installation. These files are crucial for hosting games directly from game map files. Aura 
does all the work for you, making sure everything is set up correctly behind the scenes.

**Why Use `--extract-jass`?**
- **Convenience**: With this flag, you don't need to worry about manually obtaining files. Aura takes care of it for you.
- **Saves Time**: No need to spend extra time finding and extracting files yourself.
- **Ensures Compatibility**: Extracting the necessary files ensures compatibility with a wide range of game versions.

## `--no-extract-jass`

By using this flag, you're indicating that you prefer to handle the extraction of necessary files 
manually. Although Aura won't do it for you automatically, you still have the flexibility 
to obtain and manage these files as you see fit.

**Why Use `--no-extract-jass`?**
- **Manual Control**: Some users prefer to have full control over file management.
- **Customization**: You might have specific preferences or procedures for obtaining these files.
- **Already Extracted**: If you've already extracted the files or prefer a different method, this 
flag allows you to bypass automatic extraction.

**Note**: Even if JASS files are unavailable, you can still host games using map config 
files (``.cfg``). These flags simply determine how files necessary for hosting from map files 
(``.w3x``, ``.w3m``) are obtained and managed.

# Parameters

## `--homedir <DIRECTORY>`

Specifies the directory to be used as Aura's home directory. Paths in the config file are resolved relative to the home directory.

- This flag takes precedence over any other method of determining the home directory, including environment variables.
- If `--homedir` is not provided, the environment variable `AURA_HOME` is used to determine the home directory.
- If neither `--homedir` nor the `AURA_HOME` environment variable are set, the home directory defaults to the directory where Aura's executable is located.

## `--config <FILE>`

Specifies the location of Aura's main configuration file.

- If `<FILE>` does not contain any slashes, it is resolved relative to the home directory by default, unless overridden by the `--stdpaths` flag.
- The presence of any slashes causes `<FILE>` to be resolved relative to the current working directory (CWD).
- When using `--config`, if the configuration file is not located within the home directory, the bot may only start up if the configuration file includes the entry `<bot.home_path.allow_mismatch = yes>`.

**Note**: Paths in any configuration file are resolved relative to Aura's home directory.

Defaults to ``config.ini`` in the home dir.

## \`--w3version <NUMBER>\`

Specifies the version that is to be used when hosting Warcraft 3 games. There is no cross-version 
compatibility. This parameter allows Aura to switch versions on the spot.

This option is equivalent to ``<game.version>`` in \`config.ini\`

## \`--w3dir <DIRECTORY>\` 

Specifies the directory where the Warcraft 3 game files are located. This parameter allows Aura 
to identify and fetch files from alternative Warcraft 3 game installations on the spot.

This option is equivalent to ``<game.install_path>`` in \`config.ini\`

## \`--mapdir <DIRECTORY>\`

Specifies the directory where Warcraft 3 maps are stored. This parameter allows Aura to locate  
and load maps for hosting games.

This option is equivalent to ``<bot.maps_path>`` in \`config.ini\`

## \`--cfgdir <DIRECTORY>\`

Specifies the directory path where Aura reads metadata files for Warcraft 3 maps. 
These metadata files, also known as "map config" files or mapcfg, are essential 
for hosting games, and can be used as the source of truth even if the original map 
files are unavailable.

This option enables users to specify a custom directory for Aura to locate existing map 
metadata. It's particularly useful for organizing and accessing map configurations 
separately from other application data.

This option is equivalent to ``<bot.map_configs_path>`` in \`config.ini\`

## \`--cachedir <DIRECTORY>\`

Specifies the directory path where Aura automatically generates and stores cache files 
for Warcraft 3 maps. These cache files serve the same purpose as user-created metadata 
files, but are generated by the application to optimize performance.

Separating cache files from user-created files allows for better organization and 
maintenance of map metadata.

This option is equivalent to ``<bot.map_cache_path>`` in \`config.ini\`

## \`--jassdir <DIRECTORY>\`

Specifies the directory where Warcraft 3 script files are stored. These files are needed in order 
to host games from map files (``.w3x``, ``.w3m``).

This option is equivalent to ``<bot.jass_path>`` in \`config.ini\`

## \`--savedir <DIRECTORY>\`

Specifies the directory where Warcraft 3 save files (``.w3z``) are stored.

This option is equivalent to ``<bot.save_path>`` in \`config.ini\`


## \`--lan-mode <MODE>\`

Specifies how hosted games available for "Local Area Network" should be made known to potential players.

**Options:**

- strict: Exclusively uses back-and-forth communication with interested game clients. Aura's server sends 
tiny periodic messages over the network that prompts open Warcraft III clients to request further 
information. Once Aura provides them with that information, game clients may join your hosted game.

This option is equivalent to ``<net.game_discovery.udp.broadcast.strict = yes>`` in \`config.ini\`

- lax: Aura periodically sends the full information needed to join a hosted game to all machines in the 
same network. Additionally, it will reply to the information request that happens when a game client first 
opens the "Local Area Network" menu.

This option is equivalent to ``<net.game_discovery.udp.broadcast.strict = no>`` in \`config.ini\`

- free: Aura periodically sends the full information needed to join a hosted game to all machines in the 
same network. It will not reply to any information requests, nor process any UDP traffic. Port 6112 is not 
used by Aura's UDP server. This is the only UDP mode that allows a Warcraft III game client in the host 
machine to connect to the Local Area Network.

This option is equivalent to ``<net.udp_server.enabled = no>`` in \`config.ini\`

## \`--log-level <LEVEL>\`

Specifies the level of detail for logging output.

Values:
 - trace2: Extremely detailed information, typically used for deep debugging purposes.
 - trace: Detailed information, providing a high level of insight into the bot's internal operations.
 - debug: Fine-grained informational events that are most useful for debugging purposes.
 - info: Confirmation that things are working as expected.
 - notice: Normal but significant events that may require attention.
 - warning: An indication that something unexpected happened, or indicative of some problem in the near future.
 - error: Due to a more serious problem, the software has not been able to perform some function.
 - critical: A serious error that may prevent the bot from functioning correctly.

By default, the logging level is set to info.

## \`--port-forward-tcp <PORT>\`, \`--port-forward-udp <PORT>\`

The --port-forward-tcp <PORT> and --port-forward-udp <PORT> flags are used to trigger 
Universal Plug and Play (UPnP) requests from the command line interface (CLI) for TCP and UDP 
port forwarding, respectively. These flags facilitate the hosting and discovery of multiplayer games 
by allowing incoming connections on specific ports.

  TCP (Transmission Control Protocol):
      The --port-forward-tcp <PORT> flag initiates UPnP requests to forward TCP traffic on the specified port.
      TCP is used for establishing connections between the game server and multiple clients for multiplayer gameplay.
      Enabling TCP port forwarding ensures that incoming TCP connections can reach the game server, allowing players to join multiplayer games seamlessly.

  UDP (User Datagram Protocol):
      The --port-forward-udp <PORT> flag initiates UPnP requests to forward UDP traffic on the specified port.
      UDP is utilized for server and client communication to discover hosted games and exchange game data efficiently.
      Enabling UDP port forwarding ensures that UDP packets related to game discovery and communication can reach the game server and clients.

**Note:**
- Ensure that the specified ports are not blocked by firewalls and are available for use.
- UPnP support must be enabled on the router for these requests to be successful.
- Some routers may require user authentication for UPnP requests.

# Flags for CLI games

## \`--check-joinable\`, \`--no-check-joinable\`

This flag enables automatic connectivity checks to ensure hosted games are joinable from the Internet.

## \`--check-version\`, \`--no-check-version\`

This flag enables version checks to ensure hosted games are compatible with running game version.

## \`--check-reservation\`, \`--no-check-reservation\`

This flag enables reservation checks to ensure only players with reservations may join games.

## \`--replaceable\`, \`--no-replaceable\`

This flag enables users to use the !host command to replace the hosted game by another one.

## \`--random-races\`

This flag enables randomization of player races in the hosted game. When this flag is used, each player's 
race will be randomly assigned at the start of the game session. This adds an element of unpredictability 
and variety to the game, as players may need to adapt their strategies based on their randomly assigned race.

## \`--random-heroes\`

This flag enables randomization of player heroes in the hosted game. With this flag, each player's hero will 
be randomly selected at the beginning of the game session. Randomizing heroes adds excitement and diversity 
to the gameplay, as players must adapt their tactics based on the strengths and abilities of their randomly 
assigned hero.

# Parameters for CLI games

## \`-s <TYPE>, --search-type <TYPE>\`

Specifies the type of hostable being referenced. This parameter helps Aura determine how to resolve 
the input when hosting maps from the CLI, whether they are maps, configuration files, or remote resources.

**Options:**

- map: Indicates that the specified file is a Warcraft 3 map.
- config: Indicates that the specified file is a map config/metadata file.
- local: Indicates that both maps and config files are allowed, but only in the local machine.
- any: Indicates that the specified file may be found in the cloud.

## \`--exclude <SERVER>\`

This makes hosted games invisible in the specified server. The server is to be specified through their 
unique `Input ID`, which corresponds to ``realm_N.input_id`` in \`config.ini\`. Note that this value may be 
missing, and thus defaults to ``realm_N.unique_name`` (which itself defaults to \`realm_N.host_name\`).

## \`--alias <ALIAS>\`

This option lets Aura automatically register an alias for the map hosted. Aliases are case-insensitive, and 
normalized according to the rules listed in \`aliases.ini\`.

## \`--mirror <IP:PORT#ID>\`

This option sets Aura to use game mirroring mode. In this mode, the bot won't host games by itself, but 
instead repost a game hosted elsewhere to connected Battle.net/PvPGN realms. The actual host is identified 
by their IPv4 address and PORT. The game ID, also known as "host counter", must also be provided.

Aura will remain in game mirroring mode until the process finishes.

## \`--observers <OBSERVER>\`

This parameter sets the observer mode for the hosted game. Observer mode determines how spectators are 
allowed to observe the game.

**Options:**

- none: No observers allowed. Spectators cannot join the game.
- full observers: Allows spectators to exclusively observe the game, and chat among themselves.
- referees: Allows spectators to observe the game, and communicate with the players, either in public chat, or private chat.
- observers on defeat: Players that lose the game are allowed to remain in the game, as spectators.

## \`--visibility <VISIBILITY>\`

This parameter sets the visibility mode for the hosted game, determining how much of the map is visible 
to players and observers.

**Options:**

- default: Standard visibility mode where players can only see areas of the map revealed by their units and structures.
- hide terrain: Hides terrain from players and observers, making the entire map appear as black fog of war.
- map explored: Reveals the entire map to players, allowing them to see all terrain but not enemy units or structures unless they have vision of them.
- always visible: Grants players and observers full visibility of the entire map, including enemy units and structures, without the need for vision.

## \`--owner <USER@SERVER>\`

This parameter specifies the owner of the hosted game. The owner is typically the user who has 
administrative control over the game session. Here's the format for specifying the owner:

- <USER>: The username of the owner.
- <SERVER>: The server the user is registered in.

## \`--lobby-timeout <TIME>\`

This parameter specifies the maximum time a game lobby is allowed to be unattended, that is,
without a game owner. After this time passes, the lobby is unhosted.

- <TIME>: Provided in seconds.

## \`--download-timeout <TIME>\`

This parameter specifies the maximum time a map download is allowed to take. Once this time is
exceeded, the map download and game hosting are cancelled.

- <TIME>: Provided in seconds.

## \`--load <FILE>\`

Specifies the location of a saved game a game lobby will resume.

- If `<FILE>` does not contain any slashes, it is resolved relative to the saved games directory by default, unless overridden by the `--stdpaths` flag.
- The presence of any slashes causes `<FILE>` to be resolved relative to the current working directory (CWD).

## \`--reserve <PLAYER>\`

Makes a reservation for a player to join the game lobby. This is required for loaded games to properly work.

# Flags for CLI commands

## \`--exec-broadcast\`

This flag enables broadcasting the command execution to all users or players in the channel. 
When activated, the command specified by --exec will be broadcasted to all users in the same 
realm or game, ensuring transparency and visibility of the executed action.

By using this flag, users or players will be informed about the command being executed, fostering 
a collaborative and informed environment during the game session.

# Parameters for CLI commands

## \`--exec <COMMAND>\`

This parameter allows the execution of a specific command after the bot start up, and the CLI game (if any) 
has been created. Here's how it works:

- <COMMAND>: The command to be executed. The command token is not to be included (e.g. ! or .)

## \`--exec-as <USER@SERVER>\`

This parameter specifies the user and server on which the command specified by --exec should be executed. 
It allows the execution of commands as a specific user on a particular server. Here's the format:

- <USER>: The username to whom the command is attributed.
- <SERVER>: The server the user is registered in.

## \`--exec-auth <AUTH>\`

This parameter sets the authentication mode for executing commands specified by --exec.

**Options:**

- spoofed: Treats the user as unverified by their respective server.
- verified: Treats the user as verified by their respective server.
- admin: Treats the user as an admin of their server.
- rootadmin: Treats the user as a root admin of their server.
- sudo: Treats the user as a bot sudoer. These are the highest privileges.

## \`--exec-scope <SCOPE>\`

This parameter determines where the command specified by --exec will be run.

**Options:**

- none: The command runs without any specific scope. This is the default behavior.
- lobby: The command runs in the currently hosted game lobby.
- server: The command runs in the user's realm or server.
- game#<GAME>: The command runs in the game with the specified ID.

Choose the appropriate scope based on where you want the command to be executed.


[1]: https://gitlab.com/ivojulca/aura-bot
[2]: https://gitlab.com/ivojulca/aura-bot/NETWORKING.md
[3]: https://owasp.org/www-community/attacks/Path_Traversal
