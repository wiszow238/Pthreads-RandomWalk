Typing "make" will compile both the parallel and serial program. 

Run commands:

To run the serial program type:
$ ./serial <graphFile> <walkLength> <Number of highest visited>

To run the parallel program type:
$ ./pagerank <graph file> <walkLength> <Number of highest visited>

Example:
$ ./serial 4M-graph.txt 10 10
$ ./pagerank 4M-graph.txt 10 10

The results will be printed out in "pagerank.result".

I start my time right before I start all walkers. The start time can be found on line 228. Before line 228, the data structures are created and populated. Once everthing is populated then the walkers are run, which is where the timer is started. 

The timer ends right when all walkers have reached there walk length. The finish can be found on line 238.