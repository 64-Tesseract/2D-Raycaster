# 2D Raycaster Engine
A terminal-based non-Euclidean maze exploring simulator.

## Compilation
Make sure you have `gcc` and `ncurses` for C installed. Then, run the `make.sh` script with `3d_sim.c` as the first argument and the output file as the second. For example:  
`./make.sh ./3d_sim.c 3d_sim`

_Note: I only recently started using C, so I'm no expert with compiling. If something goes wrong, you're likely missing a library; compare the `#include` statements with any error messages._  
_The provided pre-compiled executable may or may not work_ ¯\\\_(ツ)\_/¯

## Playing
#### Maps
To load a map, run the executable in a terminal ([st](https://st.suckless.org/) seems to work well) and pass a plaintext file as a parameter for the map data:  
`./3d_sim ./maps/test.txt`

Maps can be no larger and no smaller than 32*32 characters - if you wish to create a smaller map, you can fill the map with spaces. Walls are represented by capital letters (and ~), and 2D items/entities are represented by numbers 0-9. Most other characters are ignored and treated as a floor space.  
Portals, used for the non-Euclidean illusion, are represented with the characters `^`, `v`, `<`, and `>`. These "point" to another portal in the opposite direction, and the character will be able to see and walk through them as if there was no space between them.

For example, the following will appear to be a 5-unit wall from the outside, but looking inside it will appear and function as two flat surfaces:  
```
AAAAA
>   <
BBBBB
```

#### Items
Items themselves have no function besides rendering a 2D sprite in the world.

Like maps, the required size is 32*32 characters. Space will not render anything, so walls may be visible behind them. The `~` character will render an actual space, and `` ` `` will render a space that cannot be overridden by the "anti-aliasing" algorithm, which may be useful in creating detail.


#### Gameplay
The player may move with the WASD keys and rotate with J and L. Due to `ncurses`' limitations, only one key may be held at a time. I and K can be used to adjust the FOV, from 0 to 360 degrees, and M can be used as a minimap.  
The "goal" of the game would be to escape the mazes provided, without using the minimap. In previous versions the exit was represented with `O` walls, but now a door item sprite is used.