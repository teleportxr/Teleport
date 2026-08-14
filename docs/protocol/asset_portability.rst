.. _asset_portability:

#################
Asset Portability
#################

.. note::

   This page is a **design sketch**. Only the avatar case is specified and implemented
   today, as :ref:`avatar negotiation <signaling_avatars>`; the generalised message family
   described below is not yet part of the protocol. It is documented here so that the
   avatar mechanism is understood as one instance of a general pattern rather than a
   special case.

   The word *manifest* appears both here and in :ref:`avatar_manifest`, and the two are
   related but not identical. The manifest sketched below is a Teleport-defined list of
   assets carried inside an ``asset-offer``. The avatar manifest is an external
   `Universal Manifest <https://universalmanifest.net/>`_ document, fetched from an
   address the client supplies and signed by whoever issued it. The avatar case is the
   concrete realisation of this pattern for the ``avatar`` class, and it resolves the
   question this page leaves open — where a manifest lives and who vouches for it — by
   answering "somewhere else, and its issuer".

A *portable asset* is content the client brings to a world, rather than content the world
supplies. An avatar is the obvious example, but the same shape applies to an object a user
carries between worlds, or a collection of them held in an inventory.

Portability is negotiated, never assumed. A server advertises which classes of portable
asset it will accept and on what terms; a client offers what it holds, along with evidence
of its right to use it; the server accepts, constrains or refuses. A world that supports no
portable assets at all is a complete and valid Teleport server, and a client that brings
none is a complete and valid Teleport client.

Classes of portable asset
=========================

.. list-table::
   :widths: 12 40
   :header-rows: 1

   * - Class
     - Meaning
   * - ``avatar``
     - The representation of the user themselves. At most one is active per client. Specified today as :ref:`avatar negotiation <signaling_avatars>`.
   * - ``borne``
     - An object the user is holding, wearing or otherwise carrying into the world. Visible to peers from the moment it is accepted.
   * - ``stashed``
     - An object held in an inventory and not currently instantiated in the scene. It costs the world nothing until the user produces it, at which point it becomes ``borne``.

A server may support any subset. Supporting ``stashed`` without ``borne`` is legal but
pointless, since nothing could ever be produced.

Negotiation
===========

The exchange mirrors :ref:`avatar negotiation <signaling_avatars>`, generalised by an
explicit ``class``. The server may open it at any time after ``connect-response``, and may
re-open it during a session — a world can begin permitting borne objects only after the
user reaches some part of it.

.. mermaid::

    sequenceDiagram
        participant C as Client
        participant S as Server
        S->>C: asset-policy (classes, requirements, proof schemes)
        C->>S: asset-offer (manifest + proofs)
        S->>C: asset-result (per item: accepted / constrained / rejected)
        Note over S,C: asset appears to peers as ordinary nodes
        S->>C: asset-revoke (optional, at any time)

``asset-policy``
^^^^^^^^^^^^^^^^

Server to client. States what the world will accept. ``requirements`` is a free-form bag
whose keys depend on the class; unknown keys MUST be ignored, so the set can grow without a
version bump.

  .. code-block:: JSON

    { "teleport-signal-type": "asset-policy",
      "content": { "policy_id": 12345,
                   "classes": {
                     "avatar": { "requirement": "optional" },
                     "borne":  { "requirement": "optional", "max_concurrent": 2 },
                     "stashed": { "requirement": "forbidden" } },
                   "requirements": { "formats": ["glb", "vrm"],
                                     "max_file_bytes": 8388608,
                                     "max_triangles": 60000 },
                   "proof": { "required": true,
                              "accepted_schemes": ["jws-detached"] } } }

``asset-offer``
^^^^^^^^^^^^^^^

Client to server. Carries a **manifest** — the assets the client wishes to use — together
with the proofs of its right to use them. A manifest may describe one item or many; an
inventory is offered as a single manifest rather than as a message per object.

Proofs sit beside the items rather than at the top of the offer, and each names the assets
it covers. A manifest may therefore mix assets vouched for by different issuers, and one
proof may cover a group without being repeated per item.

  .. code-block:: JSON

    { "teleport-signal-type": "asset-offer",
      "content": { "policy_id": 12345,
                   "manifest": {
                     "items": [
                       { "asset_id": "urn:example:sword:42", "class": "borne",
                         "url": "https://assets.example.com/sword.glb",
                         "content_hash": "sha256:abcd",
                         "declared": { "format": "glb", "file_bytes": 40960,
                                       "triangles": 5200 } } ],
                     "proofs": [
                       { "scheme": "jws-detached", "value": "eyJ...",
                         "covers": ["urn:example:sword:42"] } ] },
                   "allow_relay": true } }

``asset-result``
^^^^^^^^^^^^^^^^

Server to client. The server is free to accept an offer only in part.

  .. code-block:: JSON

    { "teleport-signal-type": "asset-result",
      "content": { "policy_id": 12345,
                   "items": [
                     { "asset_id": "urn:example:sword:42",
                       "status": "accepted_with_constraints",
                       "node_uid": 999, "delivery": "import",
                       "constraints": { "scale_clamped": true },
                       "reasons": ["oversize_reduced"] },
                     { "asset_id": "urn:example:lantern:7",
                       "status": "rejected",
                       "reasons": ["proof_missing"] } ] } }

``status`` is ``accepted``, ``accepted_with_constraints`` or ``rejected``. ``node_uid`` and
``delivery`` carry the same meanings as in :ref:`avatar negotiation <signaling_avatars>`.

``asset-revoke``
^^^^^^^^^^^^^^^^

Server to client, at any time, naming ``asset_id``\ s that are no longer valid. A server
may revoke because a proof expired, because the user moved to a part of the world with
different rules, or for no stated reason.

Proof of right to use
=====================

Where ``proof.required`` is set, the client supplies evidence that it may use the assets it
offers. The scheme is negotiated through ``accepted_schemes``; the protocol does not
mandate one.

Binding
^^^^^^^

* A proof MUST bind the **holder** to the **assets it covers** — the connecting client's
  identity together with those assets' identifiers or content hashes. A proof over an asset
  alone is a bearer token, and can be replayed by anyone who observes it.
* The covered set MUST be enumerated **inside** the signed payload. ``covers`` in the
  message is a routing hint: a server MUST check it against the signed content and MUST NOT
  extend coverage on the client's say-so. Otherwise a proof covering ``{A, B}`` could be
  presented as covering ``{A, B, C}``.
* Servers MUST reject a proof whose subject is not the connecting client.

Coverage
^^^^^^^^

Proof is evaluated **per asset**, never per offer:

* An item covered by no valid proof is unproven, and is rejected if the policy requires
  proof for its class. Other items in the same manifest are unaffected — partial acceptance
  is the normal outcome, not an error.
* An item MUST NOT be treated as proven merely because it arrived in a manifest alongside
  proven items. A single proof governing a whole offer would let one legitimately held
  asset carry any number of others.
* A rejected item is reported in ``asset-result`` with its own ``status`` and a reason such
  as ``"proof_invalid"`` or ``"proof_missing"``.

Proof remains optional. A world that does not care who owns what leaves ``proof.required``
unset, and portability still works.

See :ref:`identity <signaling>` for the identity a proof binds to. A proof is **never**
forwarded to peers.

Restrictions
============

A server is entitled to constrain what it accepts, and to do so for reasons that are
nothing to do with permission:

.. list-table::
   :widths: 15 40
   :header-rows: 1

   * - Ground
     - Examples
   * - Performance
     - Triangle, texture, file-size or animation budgets; a cap on concurrent borne objects; refusal of an asset that would exceed a per-client total.
   * - Features
     - A world that supports no skeletal animation, no transparency, or no emissive materials, and refuses or flattens assets that need them.
   * - Representation
     - A world with an art direction of its own may substitute a stylised stand-in, clamp scale, or restrict an object to a held position rather than free placement.

A constrained acceptance is reported with ``accepted_with_constraints`` so the owning
client can tell its user why what they brought does not look as they expect. The server is
under no obligation to explain further, and MUST NOT be assumed to have applied only the
constraints it lists.

Projection to peers
===================

**A ported asset reaches other clients as an ordinary object.** It is delivered as nodes
and resources on the geometry channel, indistinguishable from content the world itself
authored. To every other client, an avatar is an animated shape and a borne object is a
shape parented to it.

This is the point of the design rather than an implementation convenience:

* No signalling message tells a client anything about another client's portable assets. As
  with avatars, every message on this page concerns the recipient's **own** assets.
* Peers need no code for portability. A client that has never heard of inventories renders
  a world full of ported objects correctly, because there is nothing to render differently.
* The server remains authoritative over the scene. A ported asset that has been accepted is
  the server's node, subject to the same lifetime, visibility and update rules as any
  other, and may be removed at any time.
* Provenance is not carried. Peers cannot learn where an asset came from, who vouched for
  it, or what it cost — so portability adds no new privacy surface between users.
