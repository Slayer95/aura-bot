Config
==========
# Supported config keys
## \`bot.exit_on_standby\`
Type: bool
Default value: false
Error handling: Use default value

## \`bot.greeting_path\`
Type: path
Error handling: Use default value

## \`bot.latency\`
Type: uint16
Default value: 100
Error handling: Use default value

## \`bot.load_maps.cache.enabled\`
Type: bool
Default value: true
Error handling: Use default value

## \`bot.load_maps.strict_paths\`
Type: bool
Default value: false
Error handling: Use default value

## \`bot.map_configs_path\`
Type: path
Default value: Empty
Error handling: Use default value

## \`bot.maps_path\`
Type: path
Default value: Empty
Error handling: Use default value

## \`bot.max_persistent_size\`
Type: int
Default value: 4294967295
Error handling: Use default value

## \`bot.perf_limit\`
Type: int
Default value: 150
Error handling: Use default value

## \`bot.toggle_every_realm\`
Type: bool
Error handling: Use default value

## \`game.install_path\`
Type: path
Error handling: Use default value

## \`game.version\`
Type: int
Error handling: Abort operation

## \`global_realm.allow_host_non_admins\`
Type: bool
Default value: true
Error handling: Use default value

## \`global_realm.announce_chat\`
Type: bool
Default value: true
Error handling: Use default value

## \`global_realm.auth_exe_version\`
Type: uint8vector
Default value: 173 1 27 1
Error handling: Use default value

## \`global_realm.auth_exe_version_hash\`
Type: uint8vector
Default value: 72 160 171 170
Error handling: Use default value

## \`global_realm.auth_game_version\`
Type: uint8
Default value: 27
Error handling: Use default value

## \`global_realm.auth_password_hash_type\`
Type: string
Default value: pvpgn
Error handling: Use default value

## \`global_realm.bind_address\`
Type: address
Error handling: Use default value

## \`global_realm.command_trigger\`
Type: string
Constraints: Min length: 1. Max length: 1.
Default value: !
Error handling: Abort operation

## \`global_realm.country\`
Type: string
Default value: Peru
Error handling: Use default value

## \`global_realm.country_short\`
Type: string
Default value: PE
Error handling: Use default value

## \`global_realm.custom_ip_address.enabled\`
Type: bool
Default value: false
Error handling: Use default value

## \`global_realm.custom_ip_address.value\`
Type: addressipv4
Default value: 0.0.0.0
Error handling: Abort operation

## \`global_realm.custom_port.enabled\`
Type: bool
Default value: false
Error handling: Use default value

## \`global_realm.custom_port.value\`
Type: uint16
Default value: 6112
Error handling: Abort operation

## \`global_realm.enabled\`
Type: bool
Default value: true
Error handling: Use default value

## \`global_realm.first_channel\`
Type: string
Default value: The Void
Error handling: Use default value

## \`global_realm.flood_immune\`
Type: bool
Default value: Empty
Error handling: Use default value

## \`global_realm.game_prefix\`
Type: string
Default value: Empty
Error handling: Use default value

## \`global_realm.locale\`
Type: string
Default value: system
Error handling: Use default value

## \`global_realm.map_transfers.max_size\`
Type: int
Default value: Empty
Error handling: Use default value

## \`global_realm.mirror\`
Type: bool
Default value: false
Error handling: Use default value

## \`global_realm.password\`
Type: string
Default value: Empty
Error handling: Use default value

## \`global_realm.root_admins\`
Type: set
Default value: Empty
Error handling: Use default value

## \`global_realm.sudo_users\`
Type: set
Default value: Empty
Error handling: Use default value

## \`global_realm.username\`
Type: string
Default value: Empty
Error handling: Use default value

## \`global_realm.vpn\`
Type: bool
Default value: false
Error handling: Use default value

## \`hosting.abandoned_lobby.game_expiry_time\`
Type: int
Default value: 10
Error handling: Use default value

## \`hosting.abandoned_lobby.owner_expiry_time\`
Type: int
Default value: 2
Error handling: Use default value

## \`hosting.command_trigger\`
Type: string
Constraints: Min length: 1. Max length: 1.
Default value: !
Error handling: Abort operation

## \`hosting.enabled\`
Type: bool
Default value: true
Error handling: Use default value

## \`hosting.game_over.player_count\`
Type: int
Default value: 1
Error handling: Use default value

## \`hosting.high_ping.kick_ms\`
Type: int
Default value: 300
Error handling: Use default value

## \`hosting.high_ping.warn_ms\`
Type: int
Default value: 200
Error handling: Use default value

## \`hosting.index.creator_name\`
Type: string
Constraints: Min length: 1. Max length: 15.
Default value: Aura Bot
Error handling: Use default value

## \`hosting.map_downloads.enabled\`
Type: bool
Default value: false
Error handling: Use default value

## \`hosting.map_missing.kick_delay\`
Type: int
Default value: 60
Error handling: Use default value

## \`hosting.map_transfers.max_parallel_packets\`
Type: int
Default value: 1000
Error handling: Use default value

## \`hosting.map_transfers.max_players\`
Type: int
Default value: 3
Error handling: Use default value

## \`hosting.map_transfers.max_size\`
Type: int
Default value: 8192
Error handling: Use default value

## \`hosting.map_transfers.max_speed\`
Type: int
Default value: 1024
Error handling: Use default value

## \`hosting.map_transfers.mode\`
Type: stringindex
Default value: auto
Error handling: Use default value

## \`hosting.max_games\`
Type: int
Default value: 20
Error handling: Use default value

## \`hosting.namepace.first_game_id\`
Type: int
Default value: 100
Error handling: Use default value

## \`hosting.self.virtual_player.name\`
Type: string
Constraints: Min length: 1. Max length: 15.
Default value: |cFF4080C0Aura
Error handling: Use default value

## \`hosting.vote_kick.min_percent\`
Type: int
Default value: 70
Error handling: Use default value

## \`irc.allow_host_non_admins\`
Type: bool
Default value: false
Error handling: Use default value

## \`irc.channels\`
Type: list
Default value: Empty
Error handling: Use default value

## \`irc.command_trigger\`
Type: string
Default value: !
Error handling: Use default value

## \`irc.enabled\`
Type: bool
Default value: false
Error handling: Use default value

## \`irc.host_name\`
Type: string
Default value: Empty
Error handling: Use default value

## \`irc.nickname\`
Type: string
Default value: Empty
Error handling: Use default value

## \`irc.password\`
Type: string
Default value: Empty
Error handling: Use default value

## \`irc.port\`
Type: uint16
Default value: 6667
Error handling: Use default value

## \`irc.root_admins\`
Type: set
Default value: Empty
Error handling: Use default value

## \`irc.username\`
Type: string
Default value: Empty
Error handling: Use default value

## \`metrics.rtt_pings\`
Type: bool
Default value: false
Error handling: Use default value

## \`net.bind_address\`
Type: addressipv4
Default value: 0.0.0.0
Error handling: Abort operation

## \`net.bind_address6\`
Type: addressipv6
Default value: ::
Error handling: Abort operation

## \`net.game_discovery.udp.broadcast.address\`
Type: addressipv4
Default value: 255.255.255.255
Error handling: Abort operation

## \`net.game_discovery.udp.broadcast.enabled\`
Type: bool
Default value: true
Error handling: Use default value

## \`net.game_discovery.udp.broadcast.strict\`
Type: bool
Default value: true
Error handling: Use default value

## \`net.game_discovery.udp.do_not_route\`
Type: bool
Default value: false
Error handling: Use default value

## \`net.game_discovery.udp.enabled\`
Type: bool
Default value: true
Error handling: Use default value

## \`net.game_discovery.udp.extra_clients.ip_addresses\`
Type: ipstringset
Default value: 
Error handling: Use default value

## \`net.game_discovery.udp.extra_clients.strict\`
Type: bool
Default value: false
Error handling: Use default value

## \`net.game_discovery.udp.ipv6.target_port\`
Type: uint16
Default value: 5678
Error handling: Use default value

## \`net.game_discovery.udp.tcp4_custom_port.enabled\`
Type: bool
Default value: false
Error handling: Use default value

## \`net.game_discovery.udp.tcp4_custom_port.value\`
Type: uint16
Default value: 6112
Error handling: Use default value

## \`net.game_discovery.udp.tcp6_custom_port.enabled\`
Type: bool
Default value: false
Error handling: Use default value

## \`net.game_discovery.udp.tcp6_custom_port.value\`
Type: uint16
Default value: 5678
Error handling: Use default value

## \`net.has_buffer_bloat\`
Type: bool
Default value: false
Error handling: Use default value

## \`net.host_port.max\`
Type: uint16
Default value: <net.host_port.min>
Error handling: Use default value

## \`net.host_port.min\`
Type: uint16
Default value: 6112
Error handling: Use default value

## \`net.ipv4.public_address.algorithm\`
Type: string
Default value: api
Error handling: Use default value

## \`net.ipv4.public_address.value\`
Type: addressipv4
Default value: 0.0.0.0
Error handling: Abort operation

## \`net.ipv4.public_address.value\`
Type: string
Default value: https://api.ipify.org
Error handling: Abort operation

## \`net.ipv6.public_address.algorithm\`
Type: string
Default value: api
Error handling: Use default value

## \`net.ipv6.public_address.value\`
Type: addressipv6
Default value: ::
Error handling: Abort operation

## \`net.ipv6.public_address.value\`
Type: string
Default value: https://api6.ipify.org
Error handling: Abort operation

## \`net.ipv6.tcp.announce_chat\`
Type: bool
Default value: true
Error handling: Use default value

## \`net.ipv6.tcp.enabled\`
Type: bool
Default value: true
Error handling: Use default value

## \`net.port_forwarding.upnp.enabled\`
Type: bool
Default value: true
Error handling: Use default value

## \`net.start_lag.sync_limit\`
Type: int
Default value: 10
Error handling: Use default value

## \`net.stop_lag.sync_limit\`
Type: int
Default value: 3
Error handling: Use default value

## \`net.tcp_extensions.gproxy_legacy.reconnect_wait\`
Type: uint8
Default value: 3
Error handling: Use default value

## \`net.tcp_extensions.gproxy.announce_chat\`
Type: bool
Default value: true
Error handling: Use default value

## \`net.tcp_extensions.gproxy.enabled\`
Type: bool
Default value: true
Error handling: Use default value

## \`net.tcp_extensions.gproxy.reconnect_wait\`
Type: uint8
Default value: 5
Error handling: Use default value

## \`net.tcp_extensions.gproxy.site\`
Type: string
Default value: https://www.mymgn.com/gproxy/
Error handling: Use default value

## \`net.tcp_extensions.udp_scan.enabled\`
Type: bool
Default value: true
Error handling: Use default value

## \`net.tcp_extensions.udp_tunnel.enabled\`
Type: bool
Default value: true
Error handling: Use default value

## \`net.udp_fallback.outbound_port\`
Type: uint16
Default value: 6113
Error handling: Use default value

## \`net.udp_ipv6.enabled\`
Type: bool
Default value: true
Error handling: Use default value

## \`net.udp_ipv6.port\`
Type: uint16
Default value: 6110
Error handling: Use default value

## \`net.udp_redirect.enabled\`
Type: bool
Default value: false
Error handling: Use default value

## \`net.udp_redirect.ip_address\`
Type: address
Default value: 127.0.0.1
Error handling: Abort operation

## \`net.udp_redirect.ip_address\`
Type: addressipv4
Default value: 127.0.0.1
Error handling: Abort operation

## \`net.udp_redirect.port\`
Type: uint16
Default value: 6110
Error handling: Abort operation

## \`net.udp_redirect.realm_game_lists.enabled\`
Type: bool
Default value: false
Error handling: Use default value

## \`net.udp_server.block_list\`
Type: ipstringset
Default value: 
Error handling: Use default value

## \`net.udp_server.enabled\`
Type: bool
Default value: false
Error handling: Use default value

## \`realm_N.allow_host_non_admins\`
Type: bool
Default value: <global_realm.allow_host_non_admins>
Error handling: Use default value

## \`realm_N.announce_chat\`
Type: bool
Default value: <global_realm.announce_chat>
Error handling: Use default value

## \`realm_N.auth_exe_version\`
Type: uint8vector
Default value: <global_realm.auth_exe_version>
Error handling: Use default value

## \`realm_N.auth_exe_version_hash\`
Type: uint8vector
Default value: <global_realm.auth_exe_version_hash>
Error handling: Use default value

## \`realm_N.auth_game_version\`
Type: uint8
Default value: <global_realm.auth_game_version>
Error handling: Use default value

## \`realm_N.auth_password_hash_type\`
Type: string
Default value: <global_realm.auth_password_hash_type>
Error handling: Use default value

## \`realm_N.bind_address\`
Type: address
Error handling: Use default value

## \`realm_N.canonical_name\`
Type: string
Default value: <global_realm.canonical_name>
Error handling: Use default value

## \`realm_N.cd_key.roc\`
Type: string
Default value: <global_realm.cd_key.roc>
Error handling: Use default value

## \`realm_N.cd_key.tft\`
Type: string
Default value: <global_realm.cd_key.tft>
Error handling: Use default value

## \`realm_N.command_trigger\`
Type: string
Constraints: Min length: 1. Max length: 1.
Default value: <global_realm.command_trigger>
Error handling: Abort operation

## \`realm_N.country\`
Type: string
Default value: <global_realm.country>
Error handling: Use default value

## \`realm_N.country_short\`
Type: string
Default value: <global_realm.country_short>
Error handling: Use default value

## \`realm_N.custom_ip_address.enabled\`
Type: bool
Default value: <global_realm.custom_ip_address.enabled>
Error handling: Use default value

## \`realm_N.custom_ip_address.value\`
Type: addressipv4
Error handling: Abort operation

## \`realm_N.custom_port.enabled\`
Type: bool
Default value: <global_realm.custom_port.enabled>
Error handling: Use default value

## \`realm_N.custom_port.value\`
Type: uint16
Default value: <global_realm.custom_port.value>
Error handling: Abort operation

## \`realm_N.db_id\`
Type: string
Default value: <global_realm.db_id>
Error handling: Use default value

## \`realm_N.enabled\`
Type: bool
Default value: <global_realm.enabled>
Error handling: Use default value

## \`realm_N.first_channel\`
Type: string
Default value: <global_realm.first_channel>
Error handling: Use default value

## \`realm_N.flood_immune\`
Type: bool
Default value: <global_realm.flood_immune>
Error handling: Use default value

## \`realm_N.game_prefix\`
Type: string
Default value: <global_realm.game_prefix>
Error handling: Use default value

## \`realm_N.host_name\`
Type: string
Default value: <global_realm.host_name>
Error handling: Use default value

## \`realm_N.input_id\`
Type: string
Default value: <global_realm.input_id>
Error handling: Use default value

## \`realm_N.locale\`
Type: string
Default value: <global_realm.locale>
Error handling: Use default value

## \`realm_N.map_transfers.max_size\`
Type: int
Default value: <global_realm.map_transfers.max_size>
Error handling: Use default value

## \`realm_N.mirror\`
Type: bool
Default value: <global_realm.mirror>
Error handling: Use default value

## \`realm_N.password\`
Type: string
Default value: <global_realm.password>
Error handling: Use default value

## \`realm_N.root_admins\`
Type: set
Default value: <global_realm.root_admins>
Error handling: Use default value

## \`realm_N.sudo_users\`
Type: set
Default value: <global_realm.sudo_users>
Error handling: Use default value

## \`realm_N.unique_name\`
Type: string
Default value: <global_realm.unique_name>
Error handling: Use default value

## \`realm_N.username\`
Type: string
Default value: <global_realm.username>
Error handling: Use default value

## \`realm_N.vpn\`
Type: bool
Default value: <global_realm.vpn>
Error handling: Use default value

## \`ui.notify_joins.enabled\`
Type: bool
Default value: false
Error handling: Use default value

## \`ui.notify_joins.exceptions\`
Type: set
Default value: 
Error handling: Use default value
