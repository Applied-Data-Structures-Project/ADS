# server
Crow REST layer. The only place that knows HTTP/JSON. Keep handlers thin: parse -> call service -> serialize.
Split routes into files as it grows (routes_auth.cpp, routes_student.cpp, routes_admin.cpp); every .cpp here is compiled automatically.
