// Declare resource groups for Teleport shaders. Sfx should error if group declarations don't match this:
[ResourceGroup (0, constantbuffer={0,8}, sampler={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15)]
[ResourceGroup (1, texture={4,5,6,7})]
[ResourceGroup (2, constantbuffer={5},texture = {1,2,3})]