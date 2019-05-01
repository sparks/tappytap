# Using v6

# Run for 1 board

* Go to the v6 firmware folder `cd tappytap/firmware/v6`
* run `pio run -t upload`
* Go to the processing sketch `cd tappytap/software/testerflexv6`
* Run the processing sketch
  * From interface
  * Alternately run on command line `processing-java --sketch=path/to/testerflexv6 --run`

# Change the number of boards

* Connect your boards


3) To change number of boards
4) Change processing line 14/15 and look at comments above for connection
5) Change arduino line 5
6) Rerun 1/2