# About

SG is a SQL Game Editor framework. The goal is to use SQL to store data in a very generic fashion, that can be used in a flexible manner by your game engine, because you can always use SQL queries to create data you wish to load from.

## Why SQL?

The design of SG is the result of many lunch time discussions while working as an engine and tools developer over a decade.  

* SQL is oddly well suited to a data-oriented game engine, where you want to operate on arrays of data, because SQL itself produces arrays as outputs.
* You can  store hierarchies, using something called nested sets, which are also well suited to evaluating a hierarchy in a predictable memory manner.
* You can develop interactively together with the same database.
* SQL queries may be performed in parallel, which means the entire build process is parallel.
* SQL can maintan referential integrity, meaning you nver have to worry about common issues like e.g. a name referring to something that was deleted. SQL can either propagate the deletion through or create an error.

It's designed to be a counterpart to the existing open source tools out there, like OGRE and SDL which provide engines, but no editors.

# Design Overview

There are three basic elements in SG:

* Nodes
* Properties
* Instances

Nodes contain properties. Properties are things like integers, reals (floats), and references to other nodes.

Examples of things that might be nodes:

* transform
* rigid_object
* folder
* scene
* components
* asteroids
* tile map

Examples of properties are:

* x (real)
* y (real)
* name (text) 
* hit_points (integer)
* parent_transform (reference to transform instance id)

Instances are tables that are generated via SQL triggers to map all of the properties on all of the nodes to an instance table. So a transform node with 'x real', 'y real', 'rotation real' will become exactly that in the instance table, with some extra info like the instance id being added on.

## Naming Conflicts

In order to prevent naming conflicts and reduce confusion, where any names could conflict in a confusing manner, a _ is prepended to all of the built-in SG

That means built-in tables of:
* `_node`
* `_property`
* `_root` (built-in node type)

And built-in properties for instances like:
* `_id`

It also means that you are not allowed to begin names with `_`.

And for good measure, to make things much easier for you later on, ids are not allowed to contain spaces.

# Converting this data into the game.

SG uses a model-view-controller framework, which all about having multiple views into data. SG-Edit provides one view and a controller into the SQL database, the game is just another view.

For example, you could create a level with

* level (name text)
* transform (x real, y real, depth integer, level ref)
* box (width real, height real, transform ref, level ref)
* circle (radius real, transform ref, level ref)

Now you can process this data into whatever form you want. Because there is depth then you probably need to write them all into some relational table somehow so you can draw them all back to front (painters algorithm) or front to back with a z-buffer (reverse painters). You could do something like so:

content/_root/box.pgsql:
```
select transform.x as "f32 x", transform.y as "f32 y", transform.depth as "f32 depth" from transform
inner join box on box.transform = transform._id
where transform._id = {_id}
```

# Requirements

* cmake
* Qt5
* PostgreSQL
* SDL2 (For example game only)

# SQL Setup

You can run a local PostgreSQL server on both Windows and Linux.

Once you have installed PostgreSQL, you need to create a user name and a database for the game content.

The first time you launch SG-Edit, it will prompt you to create the built in tables it needs, just click yes.

