##pktgen_scheduler/pktgen_server

###pktgen_scheduler

pktgen_scheduler is a mult-threaded job scheduler. It can read nodes and jobs from json files and/or run in interactive mode (see section How to Use). It listens for incoming statuses from a nodes, processes the status, then starts the next job for that node. Use the -h flag to learn more about the commandline args. See the .proto files for the format of the job and status messages.

###pktgen_server

pktgen_server is a daemon meant to be run on the nodes. It listens for job requests from pktgen_scheduler and performs them. When it is done, it sends a status back to pktgen_scheduler. Right now, pktgen_server only prints the job and waits 10 seconds...this is temporary and eventually pktgen_server will actually perform the job and send back an actual status.

###How to Use
I've written a makefile that should simplify the setup process. Once you've downloaded all the requirements, head to the root directory and run the following:

To compile proto files for pkt_scheduler and pkt_server and compile pkt_server

	make clean && make all

To run the pkt_server (runs as a daemon)
	
	./server/pktgen_server

To run the pkt_scheduler with some initial data

	python ./scheduler/pktgen_scheduler.py -n data/nodes.json -j data/jobs.json

Alternatively, you can run in interactive mode as such

	vagrant@precise64:/vagrant/Q$ python ./scheduler/pktgen_scheduler.py -i 
	INFO:pktgen_scheduler:Starting job scheduler.
	INFO:pktgen_scheduler:Starting node jobs.
	INFO:pktgen_scheduler:Currently listening on (localhost, 1800) for any statuses.
	Python 2.7.3 (default, Jun 22 2015, 19:33:41) 
	[GCC 4.6.3] on linux2
	Type "help", "copyright", "credits" or "license" for more information.
	(InteractiveConsole)
	>>> q.add_node(Node('m0', '127.0.0.1'))
	>>> q.add_job('m0', Job(1, {"m": 20, "t": 30, "w": 5, "n": 15, "s_min": 200, "s_max": 800, "r": True, "l": True }))
	>>> q.add_job('m0', Job(2, {"m": 40, "t": 30, "w": 5, "n": 15, "s_min": 500, "s_max": 0, "r": True, "l": True }))
	INFO:pktgen_scheduler:Successfully sent job to node (127.0.0.1, 1729).
	INFO:pktgen_scheduler:Received status from ip 127.0.0.1.
	INFO:pktgen_scheduler:Node (127.0.0.1, 1729) successfully completed job.
	INFO:pktgen_scheduler:Successfully sent job to node (127.0.0.1, 1729).
	INFO:pktgen_scheduler:Received status from ip 127.0.0.1.
	INFO:pktgen_scheduler:Node (127.0.0.1, 1729) successfully completed job.
	INFO:pktgen_scheduler:No pending jobs for node (127.0.0.1, 1729).
	>>>


Once this is done, you should see pktgen_scheduler sending 2 jobs (from data/jobs.json) to a node (from data/nodes.json). To see if this data was received, open up /var/logs/syslog and there should be the status data that the pktgen_server daemon logged.

The daemon output should look something like:

	Mar  2 22:27:53 precise64 pktgen_server[16701]: Pktgen Server started.
	Mar  2 22:27:59 precise64 pktgen_server[16703]: m: 20 t: 30 w: 5 n: 15 s_min: 200 s_max: 800 r: 1 l: 1.
	Mar  2 22:28:09 precise64 pktgen_server[16703]: Sucessfully completed job.
	Mar  2 22:28:09 precise64 pktgen_server[16706]: m: 40 t: 30 w: 5 n: 15 s_min: 500 s_max: 0 r: 1 l: 0.
	Mar  2 22:28:19 precise64 pktgen_server[16706]: Sucessfully completed job.

The scheduler output should look something like:

	vagrant@precise64:/vagrant/Q$ python scheduler/pktgen_scheduler.py
	INFO:pktgen_scheduler:Starting job scheduler.
	INFO:pktgen_scheduler:Starting node jobs.
	INFO:pktgen_scheduler:Successfully sent job to node (127.0.0.1, 1729).
	INFO:pktgen_scheduler:Currently listening on (localhost, 1800) for any statuses.
	INFO:pktgen_scheduler:Received status from ip 127.0.0.1.
	INFO:pktgen_scheduler:Node (127.0.0.1, 1729) successfully completed job.
	INFO:pktgen_scheduler:Successfully sent job to node (127.0.0.1, 1729).
	INFO:pktgen_scheduler:Received status from ip 127.0.0.1.
	INFO:pktgen_scheduler:Node (127.0.0.1, 1729) successfully completed job.
	INFO:pktgen_scheduler:No pending jobs for node (127.0.0.1, 1729).

###Requirements
- protoc: protobuf compiler to compile proto files
- protoc-c: protobuf compiler for C
- Python Protocol Buffers runtime library

###TODO
- pktgen_server
	- actually generate traffic
	- refactor some of the code and cleanup
	- catch signals to daemon
	- setup some real daemon logging rather than logging to /var/log/syslog
	- more documentation
	- better error handling	
- pktgen_scheduler
	- handle failed jobs gracefully
	- add more logging	
	- improve command line interface
	- more documentation
- README
	- add more information



