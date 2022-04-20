Commands from Server to Client
##############################

The **Server** sends all of its commands to the **Client** in the **Client's** local units and standard.


Set Origin
----------
Reference implementation: :cpp:struct:`avs::SetPositionCommand`

.. list-table:: Set Origin
	:widths: 5 10 30
	:header-rows: 1

	* - Bytes
	  - Type
	  - Description
	* - 12
	  - 3-vector of floats
	  - position
	* - 16
	  - 4-vector of floats
	  - orientation
	* - 8
	  - uint64_t
	  - validity counter
	* - 1
	  - uint8_t
	  - set relative position?
	* - 12
	  - 3-vector of floats
	  - relative position.