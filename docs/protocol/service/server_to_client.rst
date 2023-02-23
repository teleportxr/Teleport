Commands from Server to Client
##############################

The **Server** sends all of its commands to the **Client** in the **Client's** local units and standard.


Set StageSpace Origin Node
--------------------------

Reference implementation: :cpp:struct:`teleport::core::SetStageSpaceOriginNodeCommand`

.. list-table:: Set Origin
	:widths: 5 10 30
	:header-rows: 1

	* - Bytes
	  - Type
	  - Description
	* - 8
	  - uid
	  - origin_node
	* - 8
	  - uint64_t
	  - validity counter

	  
Reference implementation: :cpp:struct:`teleport::core::AcknowledgeHandshakeCommand`
	  
.. list-table:: Acknowledge Handshake
	:widths: 5 10 30
	:header-rows: 1

	* - Bytes
	  - Type
	  - Description
	* - 8
	  - size_t
	  - visibleNodeCount
	* - 8*visibleNodeCount
	  - uid[]
	  - uid's of visible nodes