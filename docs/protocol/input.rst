.. _input:

#####
Input
#####

Teleport has a dynamic input model: the server tells the client which inputs it expects, and the client reports their values each frame. Inputs are not hard-coded into the protocol -- the server can declare anything from a single boolean button up to dozens of named axes per session.

Reference implementations:

* Wire enums — :cpp:enum:`teleport::core::InputType`, typedef ``InputId`` (``TeleportCore/InputTypes.h``).
* Server declaration — :cpp:struct:`teleport::core::SetupInputsCommand`, :cpp:struct:`teleport::core::InputDefinitionNetPacket`.
* Client mapping — :cpp:class:`teleport::client::OpenXR` (``OpenXR::OnInputsSetupChanged``).
* Client messages — :cpp:struct:`teleport::core::InputStatesMessage`, :cpp:struct:`teleport::core::InputEventsMessage`.

Negotiation: SetupInputsCommand
===============================

``SetupInputsCommand`` is a server-to-client command on the **reliable** channel. It replaces any previous input definitions wholesale; the new set takes effect immediately. The packet layout is::

    [ 1  CommandPayloadType::SetupInputs ]
    [ 2  uint16 numInputs                ]
    repeat numInputs times:
        [ 2  InputId      inputId        ]
        [ 1  InputType    inputType      ]
        [ 2  uint16       pathLength     ]
        [ pathLength bytes UTF-8 path    ]

* ``InputId`` is a session-unique 16-bit identifier the client must echo in every state and event message.
* ``InputType`` is a one-byte bitfield (see below).
* ``path`` is a regular expression matched against the client-side control path (e.g. ``"/user/hand/left/input/trigger/value"`` on OpenXR). The client decides which physical control satisfies each regex; multiple controls may match, in which case the client picks one.

Receipt of ``SetupInputsCommand`` invalidates all earlier ``InputId`` values. Clients should re-bind their controls before sending the next ``InputStatesMessage``.

InputType bitfield
==================

``InputType`` is one byte, made of the following flags:

.. list-table::
   :widths: 5 14 30
   :header-rows: 1

   * - Bit
     - Flag
     - Meaning
   * - 0
     - ``IsEvent`` (1)
     - The input fires events, not per-frame state.
   * - 1
     - ``IsReleaseEvent`` (2)
     - Use only with ``IsEvent``: an event is also generated on release.
   * - 2
     - ``IsInteger`` (4)
     - The value is integer (in practice a single bit -- pressed / not).
   * - 3
     - ``IsFloat`` (8)
     - The value is a normalised float.

The combinations the protocol uses:

.. list-table::
   :widths: 14 25
   :header-rows: 1

   * - Combination
     - Wire meaning
   * - ``IntegerState`` = 4
     - Per-frame boolean reported in ``InputStatesMessage`` binary bitfield.
   * - ``FloatState`` = 8
     - Per-frame float reported in ``InputStatesMessage`` analogue array.
   * - ``IntegerEvent`` = 5
     - Edge-triggered boolean reported in ``InputEventsMessage`` binary array.
   * - ``ReleaseEvent`` = 7
     - Edge-triggered boolean that also fires on release.
   * - ``FloatEvent`` = 9
     - Float-valued event reported in ``InputEventsMessage`` analogue array.

Motion events (2D vector, e.g. thumbstick) are encoded as a separate ``InputEventMotion`` array on ``InputEventsMessage``. They share the ``InputId`` namespace and are matched by id rather than by type bit.

Reporting: InputStatesMessage
=============================

Sent every frame on the **unreliable** channel. The header counts how many state inputs the message carries; the order matches the order of declaration in the most recent ``SetupInputsCommand``. The binary bitfield is little-endian and zero-padded to a whole number of bytes.

See :doc:`service/client_to_server` for the byte layout.

Reporting: InputEventsMessage
=============================

Sent on the **unreliable** channel only when at least one event has fired since the last frame. Carries three independent arrays — binary, analogue, motion — each prefixed by its own ``uint16`` count. ``eventID`` is a per-client monotonically-increasing 32-bit identifier the server uses to deduplicate retries.

For ``ReleaseEvent`` inputs the client emits two binary events: one with ``activated == true`` on press and one with ``activated == false`` on release.

Pose stream
===========

Controller / head poses are **not** part of the input system. They are sent through the dedicated ``ControllerPoses`` (id 3) and ``OriginPose`` (id 8) client messages on the unreliable channel. See :doc:`service/client_to_server` for details. ``SetupInputsCommand`` need not declare them; the protocol assumes the client has at least a head pose and zero or more controller poses, all expressed in the session's origin space.

Sequence
========

.. mermaid::

    sequenceDiagram
        participant S as Server
        participant C as Client
        S->>C: SetupInputsCommand (N definitions)
        C->>C: bind regex paths to local controls
        loop every frame
            C->>S: InputStatesMessage (B bits + A floats)
            opt event(s) fired
                C->>S: InputEventsMessage
            end
        end

Operational notes for implementers
==================================

* Servers should send ``SetupInputsCommand`` *before* expecting any state/event traffic; the client otherwise has no way to interpret the bit/float arrays.
* Clients with no matching control for a declared input must still emit a slot for it -- ``false`` for binary states, ``0.0`` for analogue states. Otherwise the indices in the bitfield/array will not line up with the server's expectations.
* ``InputId`` values are not stable across sessions; do not cache them.
* The regex path syntax is implementation-defined. The reference client uses ``std::regex`` against OpenXR component paths; non-OpenXR clients should expose an analogous canonical path tree.
