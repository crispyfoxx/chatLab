chat/
├── CMakeLists.txt
├── Dockerfile.server
├── Dockerfile.client
├── docker-compose.yml
├── README.md
│
├── include/
│   ├── protocol/
│   │   ├── frame.h
│   │   ├── message_type.h
│   │   ├── codec.h
│   │   └── limits.h
│   │
│   ├── net/
│   │   ├── unique_fd.h
│   │   └── socket_io.h
│   │
│   └── server/
│       ├── session.h
│       ├── session_manager.h
│       ├── dispatcher.h
│       └── handlers.h
│
├── src/
│   ├── protocol/
│   │   └── codec.cpp
│   ├── net/
│   │   └── socket_io.cpp
│   │
│   ├── server/
│   │   ├── main.cpp
│   │   ├── session.cpp
│   │   ├── session_manager.cpp
│   │   ├── dispatcher.cpp
│   │   └── handlers.cpp
│   │
│   └── client/
│       ├── main.cpp
│       ├── receiver.cpp
│       └── command_parser.cpp
│
├── tests/
│   ├── protocol_test.cpp
│   ├── fragmentation_test.cpp
│   ├── malformed_test.cpp
│   ├── large_message_test.cpp
│   └── integration_test.py
│
└── docs/
    ├── protocol.md
    └── design.md