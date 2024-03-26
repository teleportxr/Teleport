// Declare resource groups for Teleport shaders. Sfx should error if group declarations don't match this:
[ResourceGroup (0, constantbuffer={0,8})]
[ResourceGroup (1, texture={4,5,6,7})]
[ResourceGroup (2, constantbuffer={5},texture = {1,2,3})]