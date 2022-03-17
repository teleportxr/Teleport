Discovery
=========

A server usually remains in the *Discovery* state while running.

The **Discovery Socket** is opened, as a nonblocking broadcast socket. It is bound to the server's address with the **Server Discovery Port** (default 10600).

A **Client** that wants to join (the **Connecting Client**) periodically sends to the server a four-byte **Connection Request** packet containing an unsigned 32-bit integer, the **Client ID**.

The **Discovery Service** listens on the **Discovery Socket** for a **Connection Request** from a **Connecting Client**. When one is received, if the server is set to filter client addresses, the **Connecting Client**'s address is filtered, and the connection is discarded it does not match the filter.

Otherwise, the server sends the **Connecting Client** on the **Client Discovery Port** a 6-byte **Service Discovery Response** consisting of the **Client ID** (4 bytes) and the **Service Server Port** (2 bytes as an unsigned 16-bit integer). The **Server Service Port** must be different for each **Client**.

In between periodically sending its **Connection Request** packets, a **Connecting Client** listens on the **Client Discovery Port** for the **Service Discovery Response**. On receiving this, it destroys its **Discovery Socket** and starts its **Service Connection** bound to the **Server Service Port**.
