Name: Sean Lossef
RIN: 661529430
RCS: losses
Partner: None

Notes:
I print to stderr a lot in the code rather than stdout because it is the most reliable way to debug output on submitty.
Program starts listening on given port, then when it gets a request handles it in "handle_request()"
From there it binds new port and completes request in new child.