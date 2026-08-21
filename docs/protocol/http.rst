.. _http_service:

############
HTTP Service
############

The HTTP service is the third leg of the protocol. It serves bulk static assets (textures and meshes) over plain HTTP or HTTPS, separately from the real-time data transport. Its purpose is to keep large, immutable, cacheable payloads off the latency-sensitive WebRTC data channels.

Reference implementation (``teleport::server::DefaultHTTPService``, backed by `cpp-httplib <https://github.com/yhirose/cpp-httplib>`__). The service runs in a dedicated thread inside the same server process as ``ClientManager``.

Endpoint
========

* Default port: **80** (HTTP) when no certificate is provided, **443** (HTTPS) when ``certPath`` and ``privateKeyPath`` are configured. The reference server hard-codes port 80 in ``ClientManager::initialize``; alternative deployments are free to put the service behind a CDN or reverse proxy.
* Bind address: ``0.0.0.0``.
* Mount point: ``/`` mapped to the ``httpMountDirectory`` passed in ``InitialiseState``. Every file under that directory is served verbatim.
* TLS: when enabled, ``cpp-httplib``'s OpenSSL server is used. The reference build currently disables OpenSSL because of a link conflict with WebRTC; production deployments should fix this or sit the service behind an HTTPS terminator.

URL format
==========

Asset URLs are constructed by the server (reference: ``teleport::server::GeometryEncoder::encodeTexturePointer``)::

    https://<httpRoot>/<relative-path>.<extension>

* ``httpRoot`` is set on the server side by ``GeometryStore::SetHttpRoot``. It is the host (and optional path prefix) that fronts the static directory; *it is not currently transmitted to the client through* ``SetupCommand``.
* ``relative-path`` is the asset's path within the cache, derived from its uid and human-readable name.
* ``extension`` is decided by the asset writer (``.texture``, ``.mesh``, ``.basis``, ``.material``, …). The reference server registers custom MIME types for these extensions:

  .. list-table::
     :widths: 14 25
     :header-rows: 1

     * - Extension
       - MIME type
     * - ``.texture``
       - ``image/texture``
     * - ``.mesh``
       - ``image/mesh``
     * - ``.material``
       - ``image/material``
     * - ``.basis``
       - ``image/basis``

The full URL is delivered to the client *inside the geometry stream*, as the payload of a :ref:`TexturePointer or MeshPointer chunk <geometry_payload>`. Each pointer chunk begins with an ``axesStandard`` byte, followed by a ``uint16`` length and the UTF-8 URL bytes (see :ref:`texture_pointer_payload`); the client opens that URL with HTTPS and treats the reply body as the corresponding ``Texture`` or ``Mesh`` payload (without the 17-byte chunk header).

The body is served exactly as authored — the server does not convert it to the client's axes standard, as it does for geometry sent inline. The ``axesStandard`` byte on the pointer says which frame the file is in, and the client converts (see :ref:`axes_conversion`). One URL therefore serves every client whatever its axes standard, and stays cacheable.

Client behaviour
================

The reference client's geometry decoder (``teleport::clientrender::GeometryDecoder``) keeps an internal HTTP utility (``avs::HTTPUtil``) with up to **12 simultaneous HTTPS connections** and an on-disk cache rooted at ``<storage>/http_cache/``.

* If the URL inside a ``TexturePointer`` / ``MeshPointer`` already contains ``://``, it is fetched verbatim.
* Otherwise it is treated as a relative path and the cache's ``defaultURLRoot`` is prepended with the scheme ``https://``. ``defaultURLRoot`` is initialised to the cache name, which the reference client sets to the server's host name.
* Once the body is received, ``GeometryDecoder::receiveFromWeb`` decodes it as if it had arrived on the geometry channel, with the appropriate ``GeometryPayloadType``.
* The cache is content-addressed by URL; the server is expected to publish each immutable asset at a unique, stable URL.

.. _external_texture_urls:

URIs inside a fetched asset
---------------------------

A fetched glTF may reference its textures as external files, with a ``uri`` in its ``images`` array. Those URIs are relative to the **asset**, not to the client, and are resolved as the glTF spec requires (reference: ``teleport::clientrender::ResolveUrl``):

.. list-table::
   :widths: 30 34 36
   :header-rows: 1

   * - ``uri`` in the asset
     - Asset fetched from
     - Resolves to
   * - ``tex.png``
     - ``https://host/props/chair.glb``
     - ``https://host/props/tex.png``
   * - ``../shared/tex.png``
     - ``https://host/props/chair.glb``
     - ``https://host/shared/tex.png``
   * - ``/shared/tex.png``
     - ``https://host/props/chair.glb``
     - ``https://host/shared/tex.png``
   * - ``https://cdn/tex.png``
     - anywhere
     - ``https://cdn/tex.png``

The asset's own query and fragment take no part, and percent-encoding is left exactly as authored — it is part of the URL that gets requested. Note that the cache's ``defaultURLRoot`` is **not** used here: for a sub-scene cache (which is what a ``.glb`` arrived at through a ``MeshPointer`` becomes) that root is the asset's whole URL, so the resolution above must produce an absolute URL by itself.

The resolved URL identifies a texture resource, so a URL already delivered anywhere in the client's cache tree — as a ``TexturePointer``, or as an image of another asset — is that same resource and its texture is reused rather than fetched again; a URL still being fetched is waited on rather than fetched a second time. See :ref:`external_textures`, which also covers why a server must publish each file at one URL, and which cache such a texture belongs to. Failing all of that the client fetches the URL itself, through the same HTTP utility and disk cache as any other resource.

A fetch that returns no body — a 404, a dead host, a refused connection — is a failure, not an empty resource. The client reports it against the URL, so that materials waiting on the texture complete with their fallbacks rather than waiting for the session's lifetime, and the URL is left unclaimed so a later reference to it may retry.

Lifecycle
=========

The server starts the HTTP service inside ``ClientManager::initialize``, before any signaling port is opened, and stops it during ``ClientManager::shutdown``. The service is shared by every connected client; there is no per-client HTTP authentication. Treat the directory as a public, read-only mirror of the asset cache.

Sequence
========

.. mermaid::

    sequenceDiagram
        participant S as Server
        participant DC as Geometry channel
        participant H as HTTP service
        participant C as Client

        S->>DC: TexturePointer { uid, url }
        DC->>C: chunk
        C->>H: GET /<url>.texture
        H-->>C: 200 + bytes
        C->>C: decode as GeometryPayloadType::Texture
        C->>DC: ReceivedResources [uid]

Operational notes for implementers
==================================

* The client never sees an explicit "HTTP root URL" in ``SetupCommand``; either the geometry stream embeds full URLs (the production case) or you must publish the asset directory at the same host as the WebSocket signaling endpoint.
* The HTTP service is intentionally stateless. Resource expiry, ETag handling, range requests and authentication are not part of the protocol. CDNs are encouraged.
* Servers that do not need disk-backed assets may omit the HTTP service entirely; in that case every ``Texture`` and ``Mesh`` resource must be sent inline on the geometry channel.
