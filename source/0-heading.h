/** app.h - A framework for building high performance web apps.

    Abstract
    ========
    This library provides a foundation for any of my future web apps.
    It features a cooperative runtime with non-blocking I/O, memory 
    management based on arena allocation, an HTML templating engine,
    and a database to store easily transferable data.

    Table of Contents:
    ===================
    Heading  ---- Provides a high level overview of the project.

    Memory ------ Provides common data structures for storing data and the
                  implementation of our region based memory system.

    Runtime ----- Provides a cooperative runtime for our application for
                  managing memory and handling multiple processes at once.

    System ------ Provides low level integration with the os for system
                  calls, non-blocking file I/O, and environment variables.

    Network ----- Provides non-blocking tcp server, a basic HTTP interface,
                  and a path based router for handling incoming requests.

    Data -------- Provides a shallow layer of abstraction on top of existing
                  data structures to allow for reflection and serialization.

    Database ---- Provides a wrapper around the sqlite3 library to store
                  data in a unstructured way with documents-based storage.

    Template ---- Provides a templating engine that will allow us to
                  generate HTML from our data structures, inspired by Go.
           
    Application - Provides a high level interface for starting our
                  application, binding data, serving files and folders.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-01
    @version  0.1.0
*/


#ifndef APP_H
#define APP_H


/////////////
// Heading //
/////////////


#ifdef APP_IMPLEMENTATION
    #define MEMORY_IMPLEMENTATION
    #define RUNTIME_IMPLEMENTATION
    #define SYSTEM_IMPLEMENTATION
    #define NETWORK_IMPLEMENTATION
    #define DATA_IMPLEMENTATION
    #define DATABASE_IMPLEMENTATION
    #define TEMPLATE_IMPLEMENTATION
    #define APPLICATION_IMPLEMENTATION
#endif






#ifdef APP_IMPLEMENTATION


void _default_app_handler(int fd)
{
    NetworkRequest *req = network_parse_http(fd);
    if (req == NULL) return;

    (void)req;
    // TODO: Handle request
    close(fd);
}


int app_start(int port)
{
    runtime_start(network_listen(port, APP_HANDLER))
}


#endif // APP_IMPLEMENTATION
#endif // APP_H
