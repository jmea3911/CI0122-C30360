/**
 *
 *   Peterson et al. dining philosophers solution using a monitor
 *
 *   Unix
 *
 *   Code example for CI0122 Operating systems class (Francisco Arroyo)
 *
 *   2026-ii
 *
 *   NachOS version: dining philosophers monitor interface
 *
 *
 **/

#include "synch.h"

class DiningPh {

public:
    DiningPh();
    ~DiningPh();
    void pickup( long who );
    void putdown( long who );
    void print();

private:
    void test( long who );
    enum { Thinking, Hungry, Eating } state[ 5 ];
    Lock * dp;			// Locks for each method in class
    Condition * self[ 5 ];

};
