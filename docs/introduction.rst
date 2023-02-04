############
Introduction
############

TeleportVR is an application level protocol and SDK for client-server virtual reality.

This documentation consists of a decription of the network :doc:`protocol</protocol>`,
a guide to the :doc:`reference</reference/index>` implementation, and
a guide to the Teleport :doc:`Unity Server SDK</unity>`.

Think of Teleport in analogy to the World Wide Web: instead of a Web Browser, you have a Metaverse Browser (i.e. the Teleport Client); instead of a Website, you have a Virtual World (operated by a Teleport Server).
And instead of passively reading a web-page, you are an active participant in an immersive application.

Concepts
========

A Teleport Server runs a multi-user XR application. *The app lives on the server*. 

The Teleport system is a client-server simulation. The server is considered to have the ground-truth of the simulation, while the client provides
a view into that simulation of varying accuracy, depending on your latency, bandwidth and other factors.

At any time, each connected client has partial information on the simulation state.

.. list-table:: Definitions
   :widths: 15 40
   :header-rows: 1

   * - Name
     - Description
   * - Node
     - An object positioned in 3D Space. A node may have child nodes, forming a hierarchy.
   * - Resource
     - A read-only, unchanging data object that is used for rendering: a mesh, texture, material, etc.
   * - Mutable Asset
     - A data object that may be expected to change during the simulation runtime: a Node is a Mutable Asset, for example.


The server tells the client which nodes it needs to keep track of, and which assets and resources it needs. The server will send the client the asset states and resource data.

Clients generally keep only a part of the world state at any time. Two clients connected to the same server could use completely different nodes and resources, or could share some. But the server should send
only the data any client needs at a given time. In Teleport, "worlds" or "levels" are not downloaded wholesale to the client, so connection is instantaneous.

The state of the simulation is considered to be *live* and *real-time* - unlike an HTML-based system where a text-file might contain the structure of the 3D world, there is no such file in Teleport, because it's a real-time protocol that expects
the state to be constantly evolving. How the server stores its data is not specified: this is entirely implementation-dependent.

The server can grant a client control of certain nodes. For example, you might directly control the nodes representing your avatar's head or hands. But the server is the final arbiter
of the simulation.

The server determines the meaning of the client's control inputs. On connection, the server tells the client which control inputs it expects, and it's up to the client to match these up to its own hardware.