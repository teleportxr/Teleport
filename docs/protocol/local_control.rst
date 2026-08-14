.. _local_control:

######################
Local Control Protocol
######################

The headless client is two processes. ``teleportd`` is the service: it owns the live streaming connections and ticks them at 20 Hz. ``teleport_cli`` is a front end that forwards one-line commands to it and prints the replies. Between them sits the **local control protocol** described here: a line-oriented request/response protocol over a loopback TCP socket.

It is deliberately not the Teleport streaming protocol. Nothing here goes near a server; it is how a *local* operator -- a terminal, a shell script, a CI job or an LLM agent via the MCP server in ``teleport-mcp/`` -- drives a running client.

Reference implementations: framing helpers in ``HeadlessClient/ControlProtocol.h``; the listener in ``HeadlessClient/ControlServer.cpp``; the dispatcher in ``HeadlessClient/CommandProcessor.cpp``; the client side in ``HeadlessClient/cli_main.cpp``.

Why a separate process
======================

A connection outlives the thing that asked for it. An agent issues a command, reports to the user, waits for feedback and acts again -- three tool calls that may be minutes apart. A session that died with its terminal could not support that flow at all, so the streams live in a daemon and every client is transient.

Transport
=========

TCP on the loopback interface only. Default port **10510**, overridable with ``-p`` on either binary or with the ``TELEPORT_SERVICE_PORT`` environment variable.

There is no authentication. Anything that can open a loopback socket on the machine can drive the service, which is the same trust boundary as the user's own shell. Do not expose the port beyond localhost.

Framing
=======

A **request** is exactly one line of UTF-8, terminated by ``\n``, tokenised on whitespace into a verb and its arguments. There is no quoting and no escaping, so no argument may contain a space.

A **response** is zero or more dot-stuffed UTF-8 lines followed by a line containing a single ``.``::

    C: ping
    S: OK
    S: pong
    S: .

Any payload line beginning with ``.`` has one extra ``.`` prepended, SMTP-style, so the terminator is never ambiguous. Receivers strip one leading ``.`` from any line that begins with two.

The first payload line is a **status header**: either ``OK`` or ``ERROR <message>``. ``teleport_cli`` maps this onto its exit code.

The protocol is strictly one request, one response, with no message ids and no server-initiated messages. A client must not send a second command before the first response is terminated. A client that multiplexes tool calls onto one socket has to queue them.

Session state
=============

Each control connection has its own state, held by the server for as long as the socket is open:

.. list-table::
   :widths: 12 40
   :header-rows: 1

   * - State
     - Meaning
   * - selected connection
     - Which streaming connection the connection-scoped verbs act on. Set by ``connect`` and ``use``, like ``USE`` in a MySQL session. Two control clients never disturb each other's selection.
   * - output format
     - ``text`` or ``json``, set by ``format``. Text is the default.

Because this state is per-socket, it is **lost on reconnect**. A client that reconnects must re-issue ``format`` and re-select its connection. A stateless caller should send ``use <id>`` immediately before every connection-scoped command rather than relying on an earlier selection.

Output formats
==============

``format text`` (the default) renders the human-readable body. This is what a terminal, a pipe, or ``nc`` sees, and it is stable: existing scripts are not expected to change.

``format json`` renders a single compact JSON object instead::

    C: format json
    S: OK
    S: {"format":"json"}
    S: .
    C: connect 127.0.0.1:8080
    S: OK
    S: {"id":1,"url":"teleport://127.0.0.1:8080"}
    S: .

The object is always exactly one line: a compact ``dump()`` contains no raw newline, and always begins with ``{``, so it never needs dot-stuffing. The status header is unchanged -- an error still produces ``ERROR <message>``, with the body carrying ``{"error": "<message>"}``.

Both renderings are produced from the same ``CommandResult`` (``HeadlessClient/CommandResult.h``), so they cannot disagree about what happened.

.. note::
   **uids are strings.** ``avs::uid`` is 64-bit and Teleport's uids routinely exceed 2\ :sup:`53`, which a JSON number cannot carry into a JavaScript client without silent precision loss. Every ``avs::uid`` in a ``data`` object is therefore a decimal *string*, matching how ``teleport-web-client`` and ``teleport-nodejs`` already treat them. Connection ids are small ``uint32`` counters and remain numbers.

Commands
========

Connection-scoped verbs -- ``status``, ``move``, ``turn``, ``input``, ``mode``, ``geometry`` -- fail with ``ERROR no connection selected (connect first, or 'use <id>')`` when nothing is selected.

.. list-table::
   :widths: 22 38
   :header-rows: 1

   * - Command
     - ``data`` object
   * - ``ping``
     - ``{"pong": true}``
   * - ``version``
     - ``{"service": "teleportd", "protocol": 1}``
   * - ``help``
     - ``{"commands": [{"verb", "usage", "summary", "aliases"}]}``
   * - ``format <text|json>``
     - ``{"format": "json"}``
   * - ``connect <host[:port]>``
     - ``{"id": 1, "url": "teleport://..."}``
   * - ``connections`` / ``list``
     - ``{"selected": 1, "connections": [{"id", "url", "state", "connected", "selected"}]}``
   * - ``use <id>``
     - ``{"selected": 1}``
   * - ``disconnect [id]``
     - ``{"disconnected": 1}``
   * - ``status``
     - ``{"id", "state", "hasSession", "server", "port", "latencyMs", "inputsAvailable", "mode"}``
   * - ``move <x> <y> <z>``
     - ``{"position": [x, y, z]}``
   * - ``turn <qx> <qy> <qz> <qw>``
     - ``{"orientation": [qx, qy, qz, qw]}``
   * - ``input list``
     - ``{"inputs": [{"id", "type", "regexPath"}]}``
   * - ``input binary <id> <0|1>``
     - ``{"sent": {"kind": "binary", "id", "value"}}``
   * - ``input analogue <id> <v>``
     - ``{"sent": {"kind": "analogue", "id", "value"}}``
   * - ``input motion <id> <x> <y>``
     - ``{"sent": {"kind": "motion", "id", "x", "y"}}``
   * - ``mode <minimal|simulated>``
     - ``{"mode": "minimal"}``
   * - ``geometry``
     - ``{"hasCache", "nodes", "nodesRemoved", "skeletons", "resourcesReceived", "pointers", "referencedUnsent", "pendingResourceAcks", "pendingNodeAcks", "unparsed"}``
   * - ``geometry nodes``
     - ``{"hasCache", "nodes": [{"uid", "name", "type", "data", "parent", "skeleton", "materials", "animations", "animationStates", "url"}]}``
   * - ``geometry resources``
     - ``{"hasCache", "pointers": [{"uid", "type", "url"}], "missing": [uid]}``
   * - ``identity``
     - ``{"displayText", "signedIn", "provider", "subject", "email", "lastError", "providers", "pendingSignIn"}``
   * - ``signin [provider]``
     - ``{"provider": "google", "started": true}``
   * - ``signout``
     - ``{}``
   * - ``shutdown``
     - ``{"shuttingDown": true}``
   * - ``quit`` / ``exit``
     - ``{"bye": true}``

``connect`` returns as soon as the attempt is *initiated*: the id is valid immediately, but the connection completes asynchronously. Poll ``status`` until ``state`` is ``CONNECTED``. The reported states are ``UNCONNECTED``, ``OFFERING``, ``AWAITING_SETUP``, ``HANDSHAKING``, ``CONNECTED``, ``RECONNECTING`` and ``UNKNOWN``, or ``DISCONNECTED`` when no session exists yet.

``shutdown`` stops the service and every stream it holds. ``quit``/``exit`` only detach the control connection; streams keep running, which is the point of the split.

In ``geometry nodes``, ``animations`` counts the clips bound to a node, while ``animationStates`` reports what the server has most recently told it to play -- one entry per animation layer, empty until an :ref:`ApplyAnimationCommand <server_to_client>` names the node::

    {"uid": "22", "name": "Avatar", "animations": 0,
     "animationStates": [{"animation": "24", "layer": 0, "timeAtTimestamp": 0.61,
                          "speed": 0.64, "loop": true, "timestampUs": 8412000}]}

These are the command's fields as they arrived, not a simulation of playback: ``timeAtTimestamp`` is where the clip stood at ``timestampUs`` and does not advance afterwards. That is enough to check that a server is driving the animation it intends to -- note ``animations`` stays ``0`` for an avatar whose skeleton lives inside a streamed sub-scene, because the clips are bound in the sub-scene's own cache.

Signing in
----------

Sign-in uses the OAuth device-code flow: the user visits a URL on another device and types a code. The service has no browser and, when it is driven by an agent, no console anyone is reading. So ``signin`` starts the flow and returns immediately, and the prompt appears in the ``pendingSignIn`` field of subsequent ``identity`` responses::

    {"pendingSignIn": {"userCode": "ABCD-EFGH",
                       "verificationUrl": "https://www.google.com/device",
                       "expiresInSeconds": 1800}}

The field is ``null`` when nothing is pending -- null rather than absent, so a client can tell "no sign-in in progress" from "this build does not send the field". Poll ``identity`` after ``signin``.

Exit codes
==========

``teleport_cli`` returns:

.. list-table::
   :widths: 8 40
   :header-rows: 1

   * - Code
     - Meaning
   * - ``0``
     - Every command answered ``OK``.
   * - ``1``
     - At least one command answered ``ERROR``.
   * - ``2``
     - Usage error, the service could not be reached, or the connection was lost.

Versioning
==========

``version`` reports the protocol number, currently **1**. Bump it for any incompatible change to the framing, the status header, or a documented ``data`` schema. Adding a new verb, or a new field to an existing ``data`` object, is backwards compatible and does not require a bump.

Driving it by hand
==================

The protocol is plain text specifically so that it can be debugged without any Teleport code::

    printf 'format json\nping\nquit\n' | nc 127.0.0.1 10510

Sending ``quit`` over a raw socket detaches that control connection only.
