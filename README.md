# automatic-irrigations
The software is a smart state machine with 3 states: MONITORING, IRRIGATING, &amp; WAITING. It monitors for a "dry cluster" (3+ neighboring sensors). If found, it enters the IRRIGATING state, running the pump for at least 5 mins until all sensors are below the WET_THRESHOLD. It then enters a 4-hour WAITING cooldown to save water.
this project is devoloped in c++ language and working arround arduino uno 3, soil moisture sensor, relay, and 8 external input multiplexer(74HC4051)
