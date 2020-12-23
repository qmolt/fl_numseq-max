
# About

External for Max/MSP written in C, Windows version. _flnumseq~_ is a float sequencer. Receives a time period as float or signal and 3 lists: a note palette list, an index list, and a time start list. When a time start is reached, the index selects a note from the palette to output. 

It is possible to select a multigate mode, a play mode, and a bar size. The sequencer will stop when the bar size is reached.

It's functionality is very basic: 

- it doesn't sort the lists provided so it won't recognize a greater time start before another index on the sequence.


### Inlets and Outlets

(from left to right)

**Inlets**

- (float/sig~) beat time period in ms
- (list) note palette
- (list) index palette
- (list) time start sequence

**Outlets**

- (float) note
- (bang) final flag

**Messages**

- play_mode (i): 1:forward, 2:backward, 3:forward-backward, 4:backward-forward
- multigate_mode (i): 0/1
- bar_size (f): size in periods
- beats (f) (s)...: duration (beats) of subsequence, symbol starting in '<' of '1's and '0's. 
- (bang): starts sequence

------------------------------------------------------

# Versions History

**v0.1**
- first version

**v0.2**
- index wrapping on index, palette, and beat start sequences
- beats list message added 
