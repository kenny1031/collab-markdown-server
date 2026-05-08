# Concurrent Markdown Collaboration Server in C

A POSIX-based collaborative Markdown editing system implemented in C.  
The project simulates a Google Docs-style architecture where a central server maintains the authoritative document state, accepts concurrent client edits over FIFO-based IPC, applies versioned operations atomically, and broadcasts committed updates to all connected clients.