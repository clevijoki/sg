# About

SG is a gemeric video game editor based around SQL. SQL stores data in a very generic fashion, that can be compiled into any form you like.

It's designed to be a counterpart to the existing open source tools out there, like OGRE and SDL which provide engine and libraries to start, but no game editor to place or manage game data.

Games like Noita (https://noitagame.com/) which can only function because of a custom game engine, can use SG to create levels, rather than always being fractally generated roguelikes. It's fun to make game engines, but it's a drag to write tools to support them.

## Design Overview

**_SG is currently in a very early stage, and not usable for game development yet, this design overview indicates the current direction, not where it is at today._**

* All property data is stored in **components**, e.g. you might create a `PlayerController`, `AIController`, and `Character` component.
* **Components** are referenced by **entities**. You could use the previous component data to create the entities `Player`, `MushroomEnemy` and `TurtleEnemy`.
* **Entities** act as scripts to place components. They can contain _build time_ properties which can override properties in their child **components**.
* **Entities** may also contain other entities
* **Scenes** are **entities** that have been marked as such, and represent the final resources you produce to make the game function. 
* Some property types (Position2D, Position3D, Transform2D, Transform3D) will be understood by SG to produce editor widgets to drag and scale elements
* One element of "data oriented design" is that your data format has to match your problem, SQL is a good format for editing, but bad for runtime. This means there is a compiler stage that can transform your data to a runtime suitible format. This part is entirely up to you.
* The game can appear in the editor viewport by inserting an interface library which can direct how it should behave, load specific scenes, set the camera transform and pause gameplay when necessary. The game runs as a separate process, so even if it crashes it won't bring down SG Edit. An overlay that contains the SG editor widgets will be placed over top of the game. This means the game can run with the same basic update loop using the same rendering logic as it does in the final product.

# Requirements

* PostgreSQL (https://www.postgresql.org/)
* CMake (https://cmake.org/)
* Ninja Build (https://ninja-build.org/) (Optional, but it is my preferred companion to CMake)
* Qt5 (https://www.qt.io/)
* SDL2 (https://www.libsdl.org/) (For example game only)

# Setup

1. Install PostgreSQL and start it
2. you need to create a user name and a database for the game content.
```
CREATE DATABASE test_game;
```
3. Build
```
mkdir build
cd build
mkdir debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug -GNinja -Wno-dev ../../src
ninja
./editor/editor --test
```
4. The first time you launch SG Edit, it will prompt you to login, use the `test_game` database you created earlier. 
5. It will also prompt you to create the built in tables it needs, just click yes.

# Why SQL?

The design of SG is the result of many lunch time discussions while working as an engine and tools developer in the AAA industry over a decade.

* SQL queries may be performed in parallel, which means the entire build process is parallel.
* SQL can maintan referential integrity, meaning you never have to worry about common issues like e.g. a name referring to something that was deleted. SQL forces you to ensure edit actions are correct at every stage.
* Merging a SQL database is a feasible operation if you can keep a simple format
* The SQL Server as a separate process allows multiple processes to access (compiler, editor) it and maintain correctness. 
* You can develop interactively together with the same database.
* SQL is flexible enough that you can interpret the data however you want, you can produce data-oriented arrays of data or you can produce a scene graph hierarchy.

