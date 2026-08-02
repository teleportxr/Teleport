.. _client_nodes:

#################################
Origins and Client-Specific Nodes
#################################

Most nodes in a session are scenery: the server owns them, every client receives
them, and they outlive any one connection. Some are not. A node may be created
*because* a client connected -- the origin of that client's tracking space, an
avatar representing it to others -- and must then disappear when that client
goes. This page describes how such nodes are identified, who receives them, and
when they are destroyed.

.. _origin_node:

The origin node
===============

``SetOriginNodeCommand`` names one node as a client's **origin**: the origin of
that client's local tracking space, expressed as a node in the server's global
space.

The origin is not the client's avatar, and not its head. It is the frame the
client's own tracking is measured from:

* Node transforms are local to their parent, so a root node is already in global
  space (see :doc:`conventions`). The origin node is an ordinary node in that
  space, with no special treatment in the hierarchy.
* Head and controller poses travel from client to server in the client's **stage
  space** -- relative to its origin, not to the world. To place a client in
  global space, compose its reported pose with the global transform of its
  origin node:

  .. code-block:: text

     global_head = origin.global_rotation * head_stage_position + origin.global_position

* An origin is normally **stationary**, changing only intermittently: a teleport,
  boarding a vehicle, moving to another room. It is not a per-frame quantity.
  ``SetOriginNodeCommand`` is acknowledged and carries a monotonic
  ``valid_counter`` for exactly this reason -- an out-of-order or duplicated
  origin change must never take effect.

A client must always be able to see its own origin node. Its peers should
normally see it too, since it is the parent transform of everything that client
carries with it.

Ownership
=========

Every node payload carries ``holder_client_id`` (see
:doc:`geometry_payload`). Zero means the node belongs to the session as a whole.
Any other value is the ``clientID`` of the client the node was created for, and
marks the node's lifetime as bounded by that client's session.

A server may send the same node to some clients and not others. Three cases are
useful in practice:

.. list-table::
   :widths: 12 40
   :header-rows: 1

   * - Visibility
     - Sent to
   * - everyone
     - The owner and all peers. Correct for an origin node.
   * - owner only
     - Only the client named by ``holder_client_id``.
   * - peers only
     - Every client except the owner. Correct for an avatar in a first-person
       client, which would otherwise be rendered from inside.

Visibility is a server-side decision and is not signalled: a client is simply
never sent a node it may not see, and is sent a ``RemoveNodes`` payload if it
may no longer see one it already has.

Lifetime
========

A client-specific node is created when its client connects and destroyed when
that client's session ends. Destruction reaches the other clients through the
ordinary geometry channel:

.. code-block:: text

    client B disconnects
      -> server destroys the nodes whose holder_client_id is B
      -> those nodes leave every other client's streamed set
      -> each is sent RemoveNodes [ ...uids... ]

No separate notification exists, and none is needed: leaving a client's streamed
set *is* the removal. Nodes that were never sent to a given client generate no
payload for it.

Ending a session
----------------

A server should treat all of the following as ending a session:

* an explicit ``disconnect`` signal from the client;
* the signalling connection closing, whether or not a ``disconnect`` preceded it;
* the client falling silent. ``SetupCommand`` advertises
  ``idle_connection_timeout``; a server that publishes one should enforce it,
  for instance with WebSocket ping/pong.

The second and third cases matter more than they look. A client that vanishes
without a ``disconnect`` -- a crash, a closed laptop, a dropped NAT binding --
otherwise leaves its nodes in the scene indefinitely, visible to every other
client as an avatar that never moves and never leaves.

Reconnection
------------

A server may hold a departed client's nodes briefly rather than destroying them
at once, so that a client which returns within that window keeps the nodes it
already had and its peers never see it flicker out and back. This is only
possible where the returning client can be recognised as the same participant:
``clientID`` identifies a *connection*, so it will differ, and a stable identity
is required (see :doc:`signaling`). Where no such identity exists, the nodes
should be destroyed when the session ends.

See also :doc:`reconnection` for what a client is expected to re-establish.
