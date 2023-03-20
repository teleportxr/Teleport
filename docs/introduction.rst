############
Introduction
############

Teleport is an :ref:`application level<https://www.bbc.co.uk/bitesize/guides/zr3yb82/revision/6> protocol and SDK for client-server virtual, augmented and mixed reality (collectively, XR).

Think of Teleport in analogy to the World Wide Web: instead of a Web Browser, you have a Metaverse Browser (i.e. the Teleport Client);
instead of a Website, you have a Virtual World (operated by a Teleport Server).
And instead of passively reading a web-page, you are an active participant in an immersive application.

Concepts
========

A Teleport Server runs a multi-user XR application. *The app lives on the server*. 

The Teleport system is a client-server simulation.
The server is considered to have the ground-truth of the simulation, while the client provides
a view into that simulation of varying accuracy, depending on the latency and bandwidth of the connection,
hardware capabilities of the client device, and other factors.
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

The server tells the client which nodes it needs to keep track of, and which assets and resources it needs.
The server will send the client the asset states and resource data.

Clients generally keep only a part of the world state at any time. Two clients connected to the same server could use completely different nodes and resources, or could share some. But the server should send
only the data any client needs at a given time. In Teleport, "worlds" or "levels" are not downloaded wholesale to the client, so connection is instantaneous.

The state of the simulation is considered to be *live* and *real-time* - unlike an HTML-based system where a text-file might contain the structure of the 3D world, there is no such file in Teleport, because it's a real-time protocol that expects
the state to be constantly evolving. How the server stores its data is not specified: this is entirely implementation-dependent.

The server can grant a client control of certain nodes. For example, you might directly control the nodes representing your avatar's head or hands. But the server is the final arbiter
of the simulation.

The server determines the meaning of the client's control inputs. On connection, the server tells the client which control inputs it expects, and it's up to the client to match these up to its own hardware.


Comparison to the World Wide Web
================================
Although sometimes used interchangeably, the Internet and the World Wide Web are not the same network.
The Web is the subset of the Internet that consists of HTML pages, accessed via the HTTP protocol (and its extensions/successors).
The Internet is bigger - it includes email (delivered by SMTP, accessed by POP, IMAP or HTTP) and FTP, as
well as many proprietary, domain-specific or custom formats, protocols and sub-networks.

The Teleport network is part of the Internet, but it is not part of the Web.

**File Format**
The Web is founded on a file format: HTML. When you navigate to a URL, your browser expects to receive an HTML
page and render it as styled text. The whole page is downloaded, as a file. The dynamic aspects to a website are
implemented by scripts that can modify that file locally, often by requesting additional information from the server.

*Teleport* has *no file format*. What the client device receives is considered to be only a partial and temporary
representation of a simulation, whose state is subject to continuous change.

**Nouns and Verbs**
The Web is a language, and that language has nouns and verbs. The nouns are: text, tags, headings, links, and so on.
The verbs are the HTTP commands: GET, PUT, POST, and a small number of others. The Web is a system of
static resources, modified discretely.

Teleports nouns are the fundamentals of spatial computing: Nodes (hierarchic 3d objects), poses (position and orientation),
3D shapes, materials, image streams (video) and so on. And likewise the verbs are spatial, and temporal:
commands from server to client to update states; requests from client to server to update other states.
The Teleport network is a system of live simulations, modified continually.

**Statefulness**
A stateless API is one where the entire set of information needed to form a response is encoded in the query.
The Web is strongly oriented towards a stateless approach to API's, encapsulated in the REST design principles.

By contrast, Teleport's API is necessarily stateful: the data you receive from the server depends on a great many
time- and state-dependent hidden variables in the server's databases, to which the client can have no access.