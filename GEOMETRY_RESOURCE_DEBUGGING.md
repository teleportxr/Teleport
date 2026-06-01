# Geometry Resource Receiving - Diagnostic Guide

## Problem
Client receives "Sending resource" logs from server but never receives the actual geometry resources, so "Receiving resource" logs never appear on the client.

## Data Flow
```
WebRTC Data Channel → geometryQueue → avsGeometryDecoder → geometryDecoder.decode()
                                                              ↓
                                                   "Receiving resource" log
```

## Diagnostic Logging Added

We added comprehensive logging to trace data through the pipeline:

### 1. **WebRTC Network Source** (`libavstream/src/network/webrtc_networksource.cpp`)

**Log when data arrives at network layer:**
```
"WebRtcNetworkSource onMessage: Received X bytes on stream Y (id=Z)"
```
- Location: Line 827
- Tells you: Data is arriving from server on the WebRTC channel

**Log when data is written to queue:**
```
"WebRtcNetworkSource: Successfully wrote X bytes to output node for stream geometry"
```
- Location: Line 866
- Tells you: Data successfully placed in the geometry queue

**Warning if queue is full:**
```
"WebRtcNetworkSource EFP onMessage: geometry: failed to write all to output Node. Wrote X of Y bytes."
```
- Tells you: The geometry queue is full and rejecting data

### 2. **Geometry Decoder** (`libavstream/src/geometrydecoder.cpp`)

**Log when decoder reads from queue:**
```
"GeometryDecoder: Read X bytes from queue"
```
- Location: Line 137
- Tells you: Data is flowing from queue into decoder

**Warning if decoder not configured:**
```
"GeometryDecoder::process: Not configured!"
```
- Tells you: Pipeline not ready yet

**Warnings if connections are wrong:**
```
"GeometryDecoder::process: Invalid input/output!"
```
- Tells you: Pipeline links are broken

## How to Check

### Build with Logging
```bash
cmake -B build_pc_client -S .
cmake --build build_pc_client --config Release
```

### Run and Check Logs
1. Start server and client
2. Connect and begin streaming
3. **Check client console/logs for these patterns:**

```
✓ Data arriving:      "Received X bytes on stream 2 (id=80)"  [geometry stream]
✓ Data in queue:      "Successfully wrote X bytes to output node for stream geometry"
✓ Decoder reading:    "GeometryDecoder: Read X bytes from queue"
✓ Final step:         "Receiving resource (type=X, uid=Y)"  [from ClientRender/GeometryDecoder.cpp:195]
```

### Troubleshooting Checklist

- [ ] "Received X bytes" appears? → Data arriving at network layer ✓
- [ ] No "Successfully wrote"? → **Queue is full or broken connection**
- [ ] "Successfully wrote" appears? → Data in queue ✓
- [ ] No "GeometryDecoder: Read"? → **Pipeline not processing (processAsync() not running)**
- [ ] "GeometryDecoder: Read" appears? → Decoder reading ✓
- [ ] No "Receiving resource"? → **Data lost or decoder.decode() not being called**
- [ ] "Not configured" warning? → **Pipeline setup incomplete**
- [ ] "Invalid input/output" warning? → **Pipeline links broken**

## Next Steps

1. Rebuild and run
2. Check which logs appear
3. Share the logs to identify where the bottleneck is
