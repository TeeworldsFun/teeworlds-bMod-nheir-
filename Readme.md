# Bot Mod for Teeworlds
This is a server mod of Teeworlds that bring bots in Vanilla gameplay.

## Settings

|Command |  Description | Default|
| ------ | ------------ | ------ |
|`sv_bot_slots`|  Number of slots to reserve for bots | 2 |
|`sv_bot_skin`| Bot skin | default |
|`sv_bot_allow_hook`| Bots are allowed to hook | 1 |
|`sv_bot_allow_move`| Bots are allowed to move | 1 |
|`sv_bot_allow_fire`| Bots are allowed to fire | 1 |
|`sv_bot_draw_target`| Show bot target * | 0 |
|`sv_botengine_draw_graph`| Draw graph * | 0 |

\* This settings are more suited for debuging

## The AI
The AI is really simple. It can be split in two parts.
Each tick, 
 * if the target is not defined, a target is chosen among pickups and enemies (or random places if there is no need to pickup and no enemies) if 
 * it updates the path to the target (it is omniscient).
 * it checks whether firing with a weapon could damage the target (if it's a enemy) or another enemy
 * it moves
 
The bot does not retreat, and does not teamwork. 

### Weapons
At the moment, the AI predicts the trajectories of projectiles fired in several directions.
If one or more hit a enemy, then following Hammer > Laser > Grenade > ShotGun > Gun, the bot fired.

The bot doesn't care of it's own life when it fires.

### Movements
The AI uses a pathfinder that works on a graph build from a triangulation of the empty space of the map.
This graph is small so it computes on map loading all the shortest paths.
Then, the bot tries to move from a point of the path to the farther it can see.

In order to move to a point in the line of sight, the bot jump if there is a hole, goes left/right and uses randomly the hook.
When it uses the hook, the direction that would increase speed the most is chosen.

# Legacy
Copyright (c) 2015 Magnus Auvinen


This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.


Please visit http://www.teeworlds.com for up-to-date information about 
the game, including new versions, custom maps and much more.