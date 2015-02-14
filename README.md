# modulebound_allocator
A C++ allocator that frees memory in the module (shared module, executable) it was allocated in.

Useful if you want to pass STL containers between module boundaries, especially on windows when you're linking against the static runtime.

It's a fully STL compliant allocator, and captures `::operator new`, `::operator delete` and its array counterparts.
